//
// Created by Adam on 5/30/2020.
//

#include "OpusDecoderNode.h"
#include "Values.h"

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
	if(decoder != nullptr)
	{
		opus_decoder_destroy(decoder);
		decoder = nullptr;
	}

	delete [] outBuff;
	outBuff = nullptr;
}

PoolByteArray OpusDecoderNode::decode(const PoolByteArray &opusEncoded)
{
	PoolByteArray decodedPcm;

	const int numInputBytes = opusEncoded.size();
	const unsigned char *compressedBytes = opusEncoded.read().ptr();

	int byteMark = 0;

	bool done = false;
	while(!done)
	{
		// Clear the buffers
		memset(outBuff, 0, outBuffSize);

		// Parse out packet size
		Bytes4 b{0};
		for(int ii=0; ii<4; ++ii) b.bytes[ii] = compressedBytes[byteMark+ii];
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
		if(byteMark >= numInputBytes-5)
		{
			done = true;
		}

		// Decode the current opus packet
		int out_frame_size = opus_decode(decoder, inData, packetSize, outBuff, max_frame_size, 0);
		if(out_frame_size < 0)
		{
			WARN_PRINT(String().format("decoder failed: {0}", opus_strerror(out_frame_size)));
			break;
		}

		// Prep output for copy
		const unsigned char *outBytes = reinterpret_cast<unsigned char*>(outBuff);
		const int outBytesSize = out_frame_size * channels * pcm_channel_size;

		// Resize to fit the new frame
		const int initialSize = decodedPcm.size();
		decodedPcm.resize(initialSize + outBytesSize);

		// Copy the new data into the output buffer
		uint8_t *decodedBytes = decodedPcm.write().ptr();
		uint8_t *targetArea = &(decodedBytes[initialSize]);
		memcpy(targetArea, outBytes, outBytesSize);
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
