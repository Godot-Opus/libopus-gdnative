//
// Created by Adam on 5/30/2020.
//

#include "OpusDecoderNode.h"
#include "Values.h"
#include "Utils.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <cstring>

using namespace std;
using namespace opus;
using namespace godot;

OpusDecoderNode::OpusDecoderNode()
{
	frame_size = 0;
	max_frame_size = 0;

	sample_rate = DEFAULT_SAMPLE_RATE;
	pcm_channel_size = sizeof(opus_uint16);
	channels = DEFAULT_CHANNELS;
}

OpusDecoderNode::~OpusDecoderNode() = default;

void OpusDecoderNode::_ready()
{
	lock_guard <mutex> guard(decoder_mutex);
	_init_state();
}

// Must be called with decoder_mutex held
void OpusDecoderNode::_init_state()
{
	if(decoder != nullptr) return;

	frame_size = sample_rate / 50; // We want a 20ms window
	max_frame_size = frame_size * 6;

	outBuffSize = max_frame_size * channels;
	outBuff = new opus_int16[outBuffSize];
	floatOutBuff = new float[outBuffSize];

	int err;
	decoder = opus_decoder_create(sample_rate, channels, &err);
	if(err < 0)
	{
		WARN_PRINT(vformat("failed to create decoder: %s", opus_strerror(err)));
	}
}

void OpusDecoderNode::_exit_tree()
{
	lock_guard <mutex> guard(decoder_mutex);

	if(decoder != nullptr)
	{
		opus_decoder_destroy(decoder);
		decoder = nullptr;
	}

	delete[] outBuff;
	outBuff = nullptr;

	delete[] floatOutBuff;
	floatOutBuff = nullptr;
}

PackedByteArray OpusDecoderNode::decode(const PackedByteArray &opusEncoded)
{
	lock_guard<mutex> guard(decoder_mutex);

	PackedByteArray decodedPcm;

	const int numInputBytes = opusEncoded.size();

	if(numInputBytes <= 0)
	{
		WARN_PRINT("Opus Decoder: encoded input was empty");
		return decodedPcm;
	}

	// Initial output buffer size for 5 seconds of audio
	const int max_frame_size_bytes = max_frame_size * channels * pcm_channel_size;
	const int framesPerSecond = 50;
	const int initialOutputSize = max_frame_size_bytes * framesPerSecond;
	decodedPcm.resize(initialOutputSize);

	const unsigned char *compressedBytes = opusEncoded.ptr();

	// How far into the inptu buffer we are
	int byteMark = 0;
	// Keep track of how far into the output buffer we are
	int outByteMark = 0;

	bool done = false;
	while(!done)
	{
		// Clear the buffers
		memset(outBuff, 0, outBuffSize * sizeof(opus_int16));

		// Parse out packet size
		Bytes4 b{0};
		for(int ii = 0; ii < 4; ++ii) b.bytes[ii] = compressedBytes[byteMark + ii];
		const int packetSize = b.integer;

		byteMark += 4; // Move past the packet size

		// Very unintelligent sanity check to make sure our packet size header wasn't corrupt
		if(packetSize <= 0 || packetSize > 2048)
		{
			WARN_PRINT("Bad packet size, exiting.");
			break;
		}

		// Get pointer to current packet
		const unsigned char *inData = &compressedBytes[byteMark];

		byteMark += packetSize; // move past the packet

		// If this is the last packet, we will exit when we finish this pass
		if(byteMark >= numInputBytes - 5)
		{
			done = true;
		}

		// Decode the current opus packet
		int out_frame_size = opus_decode(decoder, inData, packetSize, outBuff, max_frame_size, 0);
		if(out_frame_size < 0)
		{
			WARN_PRINT(vformat("decoder failed: %s", opus_strerror(out_frame_size)));
			break;
		}

		// Prep output for copy
		const unsigned char *outBytes = reinterpret_cast<unsigned char *>(outBuff);
		const int outBytesSize = out_frame_size * channels * pcm_channel_size;

		// Copy the new data into the output buffer
		ensure_buffer_size(decodedPcm, outByteMark, outBytesSize);
		uint8_t *decodedBytes = decodedPcm.ptrw();
		uint8_t *targetArea = &(decodedBytes[outByteMark]);
		memcpy(targetArea, outBytes, outBytesSize);

		// Move the mark past the bytes we just wrote
		outByteMark += outBytesSize;
	}

	// Down size our buffer to the required size
	if(decodedPcm.size() > outByteMark+1)
	{
		decodedPcm.resize(outByteMark+1);
	}

	return decodedPcm;
}

// Must be called with decoder_mutex held. data == nullptr runs packet loss concealment
PackedVector2Array OpusDecoderNode::_decode_float_packet(const unsigned char *data, int dataSize, int maxFrames)
{
	PackedVector2Array frames;

	int out_frame_size = opus_decode_float(decoder, data, dataSize, floatOutBuff, maxFrames, 0);
	if(out_frame_size < 0)
	{
		WARN_PRINT(vformat("decoder failed: %s", opus_strerror(out_frame_size)));
		return frames;
	}

	frames.resize(out_frame_size);
	Vector2 *out = frames.ptrw();
	for(int ii = 0; ii < out_frame_size; ++ii)
	{
		out[ii] = Vector2(floatOutBuff[ii * 2], floatOutBuff[ii * 2 + 1]);
	}

	return frames;
}

PackedVector2Array OpusDecoderNode::decode_frame(const PackedByteArray &packet)
{
	lock_guard<mutex> guard(decoder_mutex);
	_init_state();

	const int packetSize = packet.size();
	if(packetSize <= 0 || packetSize > MAX_PACKET_SIZE)
	{
		WARN_PRINT("Opus Decoder: bad packet size");
		return PackedVector2Array();
	}

	return _decode_float_packet(packet.ptr(), packetSize, max_frame_size);
}

PackedVector2Array OpusDecoderNode::decode_dropped()
{
	lock_guard<mutex> guard(decoder_mutex);
	_init_state();

	return _decode_float_packet(nullptr, 0, frame_size);
}

void OpusDecoderNode::reset_stream()
{
	lock_guard<mutex> guard(decoder_mutex);
	_init_state();

	int err = opus_decoder_ctl(decoder, OPUS_RESET_STATE);
	if(err < 0)
	{
		WARN_PRINT(vformat("failed to reset decoder: %s", opus_strerror(err)));
	}
}


void OpusDecoderNode::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("decode", "opus_encoded"), &OpusDecoderNode::decode);

	ClassDB::bind_method(D_METHOD("decode_frame", "packet"), &OpusDecoderNode::decode_frame);
	ClassDB::bind_method(D_METHOD("decode_dropped"), &OpusDecoderNode::decode_dropped);
	ClassDB::bind_method(D_METHOD("reset_stream"), &OpusDecoderNode::reset_stream);
}
