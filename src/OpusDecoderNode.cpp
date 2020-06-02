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
	Godot::print("sample_rate: {0} channels: {1}", sample_rate, channels);

	frame_size = sample_rate / 50; // We want a 20ms window
	max_frame_size = frame_size * 6;

	int err;
	decoder = opus_decoder_create(sample_rate, channels, &err);
	if(err < 0)
	{
		Godot::print("failed to create decoder: {0}\n", opus_strerror(err));
	}
	else
	{
		Godot::print("Decoder created successfully\n");
	}
}

PoolByteArray OpusDecoderNode::decode(const PoolByteArray &opusEncoded)
{
	Godot::print("Decoding...");
	PoolByteArray decodedPcm;

	int numInputBytes = opusEncoded.size();
	const unsigned char *compressedBytes = opusEncoded.read().ptr();

	opus_int16 *out = new opus_int16[max_frame_size];

	int byteMark = 0;

	bool done = false;
	while(!done)
	{
		// Clear the buffers
		memset(out, 0, sizeof(opus_int16)*max_frame_size);

		// Parse out packet size
		Bytes4 b{0};
		for(int ii=0; ii<4; ++ii) b.bytes[ii] = compressedBytes[byteMark+ii];
		int packetSize = b.integer;

		byteMark += 4; // Move past the packet size

		if(packetSize <= 0 || packetSize > 2048)
		{
			Godot::print("Bad packet size, exiting.");
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
		int out_frame_size = opus_decode(decoder, inData, packetSize, out, max_frame_size, 0);
		if(out_frame_size < 0)
		{
			Godot::print("decoder failed: {0}", opus_strerror(out_frame_size));
			break;
		}

		unsigned char *bytes = reinterpret_cast<unsigned char*>(out);
		for(int ii=0; ii<out_frame_size*channels*pcm_channel_size; ++ii)
		{
			decodedPcm.append(bytes[ii]);
		}
	}

	delete [] out;

	return decodedPcm;
}


void OpusDecoderNode::_register_methods()
{
	register_property<OpusDecoderNode, int>("sample_rate", &OpusDecoderNode::sample_rate, DEFAULT_SAMPLE_RATE);
	register_property<OpusDecoderNode, int>("channels", &OpusDecoderNode::channels, DEFAULT_CHANNELS);

	register_method("_init", &OpusDecoderNode::_init);
	register_method("_ready", &OpusDecoderNode::_ready);

	register_method("decode", &OpusDecoderNode::decode);
}
