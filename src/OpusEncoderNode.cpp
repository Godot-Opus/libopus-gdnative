//
// Created by Adam on 5/30/2020.
//

#include "OpusEncoderNode.h"
#include "Values.h"
#include "Utils.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <cstring>

using namespace std;
using namespace opus;
using namespace godot;

OpusEncoderNode::OpusEncoderNode() : Node()
{
	application = OPUS_APPLICATION_VOIP;
	sample_rate = DEFAULT_SAMPLE_RATE;
	bit_rate = DEFAULT_BITRATE;
	channels = DEFAULT_CHANNELS;
	frame_size = 0;

	pcm_channel_size = sizeof(opus_uint16);
}

OpusEncoderNode::~OpusEncoderNode()
{

}

void OpusEncoderNode::_ready()
{
	lock_guard<mutex> guard(encoder_mutex);
	_init_state();
}

// Must be called with encoder_mutex held
void OpusEncoderNode::_init_state()
{
	if(encoder != nullptr) return;

	int err;

	frame_size = sample_rate / 50;
	max_frame_size = frame_size * 6;

	inputSamplesSize = frame_size * channels;
	inputSamples = new opus_int16[inputSamplesSize];

	// Create a new encoder state
	encoder = opus_encoder_create(sample_rate, channels, application, &err);
	if(err < 0)
	{
		WARN_PRINT(vformat("failed to create an encoder: %s", opus_strerror(err)));
	}

	err = opus_encoder_ctl(encoder, OPUS_SET_BITRATE(bit_rate));
	if(err < 0)
	{
		WARN_PRINT(vformat("failed to set bitrate: %s", opus_strerror(err)));
	}
}

void OpusEncoderNode::_exit_tree()
{
	lock_guard<mutex> guard(encoder_mutex);

	if(encoder != nullptr)
	{
		opus_encoder_destroy(encoder);
		encoder = nullptr;
	}

	delete [] inputSamples;
	inputSamples = nullptr;
}

void OpusEncoderNode::set_bit_rate(int p_bit_rate)
{
	lock_guard<mutex> guard(encoder_mutex);

	bit_rate = p_bit_rate;

	if(encoder != nullptr)
	{
		int err = opus_encoder_ctl(encoder, OPUS_SET_BITRATE(bit_rate));
		if(err < 0)
		{
			WARN_PRINT(vformat("failed to set bitrate: %s", opus_strerror(err)));
		}
	}
}

int OpusEncoderNode::get_bit_rate() const
{
	return bit_rate;
}

PackedByteArray OpusEncoderNode::encode(const PackedByteArray &rawPcm)
{
	lock_guard<mutex> guard(encoder_mutex);

	PackedByteArray encodedBytes;

	const int numPcmBytes = rawPcm.size();

	if(numPcmBytes <= 0)
	{
		WARN_PRINT(String("Opus Encoder: empty audio buffer, cannot encode nothing!"));
		return encodedBytes;
	}

	// Set an initial size for the output buffer
	// Ideally this will not need to be resized during most encodings
	// Then at the end we'll resize down to the size needed. Just 2 resizes
	const int defaultAudioLengthGuessSeconds = 5;
	const int packetsPerSecond = 50;
	const int initialOutputSize = (MAX_PACKET_SIZE * packetsPerSecond * defaultAudioLengthGuessSeconds);
	encodedBytes.resize(initialOutputSize);

	const unsigned char *pcm_bytes = rawPcm.ptr();

	const int bytesPerSample = pcm_channel_size * channels;
	const int availableSamples = numPcmBytes / bytesPerSample;
	int remainingSamples = availableSamples;

	int inMarkPos = 0;
	int outPos = 0;
	bool done = false;
	while(!done)
	{
		// Clear the input buffer
		memset(inputSamples, 0, inputSamplesSize*sizeof(opus_int16));

		int curFrameSize;
		if(remainingSamples >= frame_size)
		{
			curFrameSize = frame_size;
		}
		else
		{
			curFrameSize = remainingSamples;
			// We are processing the last batch of samples,
			// terminate after this pass
			done = true;
		}

		// Copy the input samples into our buffer. This is important because opus_encode() wants
		// to read a full frame_size worth of data. If we have less than a full frame at the end, it would
		// read off the end of pcmSamples. Thus we need a zeroed out buffer so it reads into empty data.
		memcpy(inputSamples, &pcm_bytes[inMarkPos], (curFrameSize * channels * pcm_channel_size));

		inMarkPos += (curFrameSize * channels * pcm_channel_size);

		// Encode the frame.
		int opusPacketSize = opus_encode(encoder, inputSamples, frame_size, outBuff, MAX_PACKET_SIZE);
		if(opusPacketSize < 0)
		{
			WARN_PRINT(vformat("encode failed: %s!", opus_strerror(opusPacketSize)));
			break;
		}

		// Prepend the frame size
		ensure_buffer_size(encodedBytes, outPos, 4);
		uint8_t *pbaData = encodedBytes.ptrw();
		Bytes4 b{opusPacketSize};
		for(int ii=0; ii<4; ++ii) pbaData[outPos+ii] = b.bytes[ii];
		outPos += 4;

		// Copy the new data into the output array
		ensure_buffer_size(encodedBytes, outPos, opusPacketSize);
		pbaData = encodedBytes.ptrw(); // We have to get this again incase ensure_buffer_size() resized the buffer
		uint8_t *targetArea = &(pbaData[outPos]);
		memcpy(targetArea, outBuff, opusPacketSize);
		outPos += opusPacketSize;

		// Record that we've processed how ever many frames we processed
		remainingSamples -= curFrameSize;
	}

	// Down size our buffer to the required size
	if(encodedBytes.size() > outPos+1)
	{
		encodedBytes.resize(outPos+1);
	}

	return encodedBytes;
}

void OpusEncoderNode::push_audio(const PackedVector2Array &frames)
{
	lock_guard<mutex> guard(encoder_mutex);
	_init_state();

	const int numFrames = frames.size();
	const Vector2 *in = frames.ptr();

	// Per-sample copy so a double precision (REAL_T_IS_DOUBLE) build stays correct
	streamBuffer.reserve(streamBuffer.size() + numFrames * channels);
	for(int ii = 0; ii < numFrames; ++ii)
	{
		streamBuffer.push_back(in[ii].x);
		streamBuffer.push_back(in[ii].y);
	}

	// Drop the oldest audio if the caller pushes without popping
	const size_t maxBufferedFloats = (size_t)MAX_BUFFERED_SAMPLES * channels;
	if(streamBuffer.size() > maxBufferedFloats)
	{
		const size_t overflow = streamBuffer.size() - maxBufferedFloats;
		streamBuffer.erase(streamBuffer.begin(), streamBuffer.begin() + overflow);
		WARN_PRINT("Opus Encoder: stream buffer overflowed, dropping oldest audio");
	}
}

bool OpusEncoderNode::has_packet()
{
	lock_guard<mutex> guard(encoder_mutex);
	_init_state();

	return (int)streamBuffer.size() >= frame_size * channels;
}

PackedByteArray OpusEncoderNode::pop_packet()
{
	lock_guard<mutex> guard(encoder_mutex);
	_init_state();

	const int floatsPerFrame = frame_size * channels;
	if((int)streamBuffer.size() < floatsPerFrame)
	{
		return PackedByteArray();
	}

	int opusPacketSize = opus_encode_float(encoder, streamBuffer.data(), frame_size, outBuff, MAX_PACKET_SIZE);

	// Consume the frame even on failure so a bad frame can't wedge the stream
	streamBuffer.erase(streamBuffer.begin(), streamBuffer.begin() + floatsPerFrame);

	if(opusPacketSize < 0)
	{
		WARN_PRINT(vformat("encode failed: %s!", opus_strerror(opusPacketSize)));
		return PackedByteArray();
	}

	PackedByteArray packet;
	packet.resize(opusPacketSize);
	memcpy(packet.ptrw(), outBuff, opusPacketSize);
	return packet;
}

void OpusEncoderNode::reset_stream()
{
	lock_guard<mutex> guard(encoder_mutex);
	_init_state();

	streamBuffer.clear();

	int err = opus_encoder_ctl(encoder, OPUS_RESET_STATE);
	if(err < 0)
	{
		WARN_PRINT(vformat("failed to reset encoder: %s", opus_strerror(err)));
	}
}


void OpusEncoderNode::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("set_bit_rate", "bit_rate"), &OpusEncoderNode::set_bit_rate);
	ClassDB::bind_method(D_METHOD("get_bit_rate"), &OpusEncoderNode::get_bit_rate);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "bit_rate"), "set_bit_rate", "get_bit_rate");

	ClassDB::bind_method(D_METHOD("encode", "raw_pcm"), &OpusEncoderNode::encode);

	ClassDB::bind_method(D_METHOD("push_audio", "frames"), &OpusEncoderNode::push_audio);
	ClassDB::bind_method(D_METHOD("has_packet"), &OpusEncoderNode::has_packet);
	ClassDB::bind_method(D_METHOD("pop_packet"), &OpusEncoderNode::pop_packet);
	ClassDB::bind_method(D_METHOD("reset_stream"), &OpusEncoderNode::reset_stream);
}
