//
// Created by Adam on 5/30/2020.
//

#include "OpusDecoderNode.h"
#include "Values.h"
#include "Utils.h"

using namespace std;
using namespace opus;
using namespace godot;

OpusDecoderNode::OpusDecoderNode() = default;

OpusDecoderNode::~OpusDecoderNode() = default;

void OpusDecoderNode::_init()
{
	frame_size = 0;
	max_frame_size = 0;

	sample_rate = DEFAULT_SAMPLE_RATE;
	pcm_channel_size = sizeof(opus_uint16);
	channels = DEFAULT_CHANNELS;
}

void OpusDecoderNode::_ready()
{
	lock_guard <mutex> guard(decoder_mutex);

	frame_size = sample_rate / 50; // We want a 20ms window
	max_frame_size = frame_size * 6;

	outBuffSize = max_frame_size * channels;
	outBuff = new opus_int16[outBuffSize];

	int err;
	decoder = opus_decoder_create(sample_rate, channels, &err);
	if(err < 0)
	{
		Godot::print("failed to create decoder: {0}\n", opus_strerror(err));
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
}

PoolByteArray OpusDecoderNode::decode(const PoolByteArray opusEncoded)
{
	lock_guard<mutex> guard(decoder_mutex);

	PoolByteArray decodedPcm;

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

	const PoolByteArray::Read read = opusEncoded.read();
	const unsigned char *compressedBytes = read.ptr();

	// How far into the inptu buffer we are
	int byteMark = 0;
	// Keep track of how far into the output buffer we are
	int outByteMark = 0;

	bool done = false;
	while(!done)
	{
		// Clear the buffers
		memset(outBuff, 0, outBuffSize);

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
			WARN_PRINT(String("decoder failed: {0}").format(Array::make(opus_strerror(out_frame_size))));
			break;
		}

		// Prep output for copy
		const unsigned char *outBytes = reinterpret_cast<unsigned char *>(outBuff);
		const int outBytesSize = out_frame_size * channels * pcm_channel_size;

		// Copy the new data into the output buffer
		ensure_buffer_size(decodedPcm, outByteMark, outBytesSize);
		uint8_t *decodedBytes = decodedPcm.write().ptr();
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


void OpusDecoderNode::_register_methods()
{
	register_method("_init", &OpusDecoderNode::_init);
	register_method("_ready", &OpusDecoderNode::_ready);
	register_method("_exit_tree", &OpusDecoderNode::_exit_tree);
	register_method("decode", &OpusDecoderNode::decode);
}
