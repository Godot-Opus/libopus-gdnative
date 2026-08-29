//
// Created by Adam on 5/30/2020.
//

#include "OpusEncoderNode.h"
#include "Values.h"
#include "Utils.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/classes/audio_server.hpp>

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
	lock_guard<mutex> guard(encoder_mutex);
	_free_state();
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
	delete [] inputSamples;
	inputSamples = new opus_int16[inputSamplesSize];

	// Create a new encoder state
	encoder = opus_encoder_create(sample_rate, channels, application, &err);
	if(err < 0 || encoder == nullptr)
	{
		WARN_PRINT(vformat("failed to create an encoder: %s", opus_strerror(err)));
		encoder = nullptr;
		return;
	}

	err = opus_encoder_ctl(encoder, OPUS_SET_BITRATE(bit_rate));
	if(err < 0)
	{
		WARN_PRINT(vformat("failed to set bitrate: %s", opus_strerror(err)));
	}

	// Opus can't represent other rates, so a mismatch silently pitch shifts the audio
	AudioServer *audio = AudioServer::get_singleton();
	if(audio != nullptr && (int)audio->get_mix_rate() != sample_rate)
	{
		WARN_PRINT(vformat("AudioServer mix rate is %d Hz but the encoder expects %d Hz; captured audio will be pitch shifted. Set audio/driver/mix_rate to match.", (int)audio->get_mix_rate(), sample_rate));
	}
}

// Must be called with encoder_mutex held
void OpusEncoderNode::_free_state()
{
	if(encoder != nullptr)
	{
		opus_encoder_destroy(encoder);
		encoder = nullptr;
	}

	delete [] inputSamples;
	inputSamples = nullptr;

	streamBuffer.clear();
	streamBufferReadPos = 0;
	overflowWarned = false;
}

// Must be called with encoder_mutex held
bool OpusEncoderNode::_has_full_frame() const
{
	return streamBuffer.size() - streamBufferReadPos >= (size_t)(frame_size * channels);
}

void OpusEncoderNode::_exit_tree()
{
	lock_guard<mutex> guard(encoder_mutex);
	_free_state();
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

void OpusEncoderNode::set_sample_rate(int p_sample_rate)
{
	lock_guard<mutex> guard(encoder_mutex);

	if(p_sample_rate != 8000 && p_sample_rate != 12000 && p_sample_rate != 16000
		&& p_sample_rate != 24000 && p_sample_rate != 48000)
	{
		WARN_PRINT("Opus supports sample rates of 8000, 12000, 16000, 24000 or 48000 Hz");
		return;
	}

	if(p_sample_rate == sample_rate) return;

	sample_rate = p_sample_rate;
	// State is recreated with the new configuration on the next call
	_free_state();
}

int OpusEncoderNode::get_sample_rate() const
{
	return sample_rate;
}

void OpusEncoderNode::set_channels(int p_channels)
{
	lock_guard<mutex> guard(encoder_mutex);

	if(p_channels != 1 && p_channels != 2)
	{
		WARN_PRINT("Opus supports 1 or 2 channels");
		return;
	}

	if(p_channels == channels) return;

	channels = p_channels;
	_free_state();
}

int OpusEncoderNode::get_channels() const
{
	return channels;
}

void OpusEncoderNode::set_application(int p_application)
{
	lock_guard<mutex> guard(encoder_mutex);

	if(p_application != OPUS_APPLICATION_VOIP && p_application != OPUS_APPLICATION_AUDIO
		&& p_application != OPUS_APPLICATION_RESTRICTED_LOWDELAY)
	{
		WARN_PRINT("application must be OPUS_APPLICATION_VOIP (2048), OPUS_APPLICATION_AUDIO (2049) or OPUS_APPLICATION_RESTRICTED_LOWDELAY (2051)");
		return;
	}

	if(p_application == application) return;

	application = p_application;
	_free_state();
}

int OpusEncoderNode::get_application() const
{
	return application;
}

PackedByteArray OpusEncoderNode::encode(const PackedByteArray &rawPcm)
{
	lock_guard<mutex> guard(encoder_mutex);
	_init_state();

	PackedByteArray encodedBytes;

	if(encoder == nullptr)
	{
		WARN_PRINT("Opus Encoder: no encoder state");
		return encodedBytes;
	}

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
	while(remainingSamples > 0)
	{
		// Clear the input buffer
		memset(inputSamples, 0, inputSamplesSize*sizeof(opus_int16));

		// A partial final frame is encoded zero padded
		const int curFrameSize = remainingSamples >= frame_size ? frame_size : remainingSamples;

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
	if(encodedBytes.size() > outPos)
	{
		encodedBytes.resize(outPos);
	}

	return encodedBytes;
}

void OpusEncoderNode::push_audio(const PackedVector2Array &frames)
{
	lock_guard<mutex> guard(encoder_mutex);
	_init_state();

	const int numFrames = frames.size();
	const Vector2 *in = frames.ptr();

	const size_t maxBufferedFloats = (size_t)MAX_BUFFERED_SAMPLES * channels;

	// Compact the consumed prefix once it grows past the cap; amortized this
	// costs one erase per cap's worth of audio instead of one per pop
	if(streamBufferReadPos >= maxBufferedFloats)
	{
		streamBuffer.erase(streamBuffer.begin(), streamBuffer.begin() + streamBufferReadPos);
		streamBufferReadPos = 0;
	}

	// Per-sample copy so a double precision (REAL_T_IS_DOUBLE) build stays correct
	streamBuffer.reserve(streamBuffer.size() + numFrames * channels);
	if(channels == 1)
	{
		// Downmix the stereo capture frames
		for(int ii = 0; ii < numFrames; ++ii)
		{
			streamBuffer.push_back((in[ii].x + in[ii].y) * 0.5f);
		}
	}
	else
	{
		for(int ii = 0; ii < numFrames; ++ii)
		{
			streamBuffer.push_back(in[ii].x);
			streamBuffer.push_back(in[ii].y);
		}
	}

	// Drop the oldest audio if the caller pushes without popping
	if(streamBuffer.size() - streamBufferReadPos > maxBufferedFloats)
	{
		streamBufferReadPos = streamBuffer.size() - maxBufferedFloats;
		if(!overflowWarned)
		{
			WARN_PRINT("Opus Encoder: stream buffer overflowed, dropping oldest audio");
			overflowWarned = true;
		}
	}
}

bool OpusEncoderNode::has_packet()
{
	lock_guard<mutex> guard(encoder_mutex);
	_init_state();

	return _has_full_frame();
}

PackedByteArray OpusEncoderNode::pop_packet()
{
	lock_guard<mutex> guard(encoder_mutex);
	_init_state();

	if(encoder == nullptr || !_has_full_frame())
	{
		return PackedByteArray();
	}

	int opusPacketSize = opus_encode_float(encoder, streamBuffer.data() + streamBufferReadPos, frame_size, outBuff, MAX_PACKET_SIZE);

	// Consume the frame even on failure so a bad frame can't wedge the stream
	streamBufferReadPos += frame_size * channels;
	if(streamBufferReadPos >= streamBuffer.size())
	{
		streamBuffer.clear();
		streamBufferReadPos = 0;
	}

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
	streamBufferReadPos = 0;
	overflowWarned = false;

	if(encoder != nullptr)
	{
		int err = opus_encoder_ctl(encoder, OPUS_RESET_STATE);
		if(err < 0)
		{
			WARN_PRINT(vformat("failed to reset encoder: %s", opus_strerror(err)));
		}
	}
}


void OpusEncoderNode::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("set_bit_rate", "bit_rate"), &OpusEncoderNode::set_bit_rate);
	ClassDB::bind_method(D_METHOD("get_bit_rate"), &OpusEncoderNode::get_bit_rate);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "bit_rate"), "set_bit_rate", "get_bit_rate");

	ClassDB::bind_method(D_METHOD("set_sample_rate", "sample_rate"), &OpusEncoderNode::set_sample_rate);
	ClassDB::bind_method(D_METHOD("get_sample_rate"), &OpusEncoderNode::get_sample_rate);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sample_rate", PROPERTY_HINT_ENUM, "8000:8000,12000:12000,16000:16000,24000:24000,48000:48000"), "set_sample_rate", "get_sample_rate");

	ClassDB::bind_method(D_METHOD("set_channels", "channels"), &OpusEncoderNode::set_channels);
	ClassDB::bind_method(D_METHOD("get_channels"), &OpusEncoderNode::get_channels);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "channels", PROPERTY_HINT_ENUM, "Mono:1,Stereo:2"), "set_channels", "get_channels");

	ClassDB::bind_method(D_METHOD("set_application", "application"), &OpusEncoderNode::set_application);
	ClassDB::bind_method(D_METHOD("get_application"), &OpusEncoderNode::get_application);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "application", PROPERTY_HINT_ENUM, "VOIP:2048,Audio:2049,Low Delay:2051"), "set_application", "get_application");

	ClassDB::bind_method(D_METHOD("encode", "raw_pcm"), &OpusEncoderNode::encode);

	ClassDB::bind_method(D_METHOD("push_audio", "frames"), &OpusEncoderNode::push_audio);
	ClassDB::bind_method(D_METHOD("has_packet"), &OpusEncoderNode::has_packet);
	ClassDB::bind_method(D_METHOD("pop_packet"), &OpusEncoderNode::pop_packet);
	ClassDB::bind_method(D_METHOD("reset_stream"), &OpusEncoderNode::reset_stream);
}
