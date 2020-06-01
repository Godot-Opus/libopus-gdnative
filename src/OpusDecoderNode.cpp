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
	Godot::print("Decoder _init\n");

	frame_size = 0;
	max_frame_size = 0;

	sample_rate = DEFAULT_SAMPLE_RATE;
	pcm_channel_size = sizeof(opus_uint16);
	channels = DEFAULT_CHANNELS;
}

void OpusDecoderNode::_ready()
{
	/*
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
	*/
}

PoolByteArray OpusDecoderNode::decode(const PoolByteArray &opusEncoded)
{
	Godot::print("Decoding...");
	PoolByteArray decodedPcm;

	int numInputBytes = opusEncoded.size();
	PoolByteArray::Read opusReader = opusEncoded.read();
	const unsigned char *compressedBytes = opusEncoded.read().ptr();


	// $TODO: max_frame_size here in incorrect, that is a PCM value.
	unsigned char *inBuff = new unsigned char[max_frame_size*channels];
	uint16_t *pcmSamples = new uint16_t[max_frame_size];
	int16_t *out = new int16_t[max_frame_size];

	int byteMark = 0;

	Godot::print("Setup complete.");

	bool done = false;
	while(!done)
	{
		// Clear the buffers
		memset(inBuff, 0, sizeof(unsigned char)*max_frame_size*channels);
		memset(pcmSamples, 0, sizeof(uint16_t)*max_frame_size);
		memset(out, 0, sizeof(uint16_t)*max_frame_size);

		// Parse out packet size
		Godot::print("Parsing packet size...");
		Bytes b{0};
		for(int ii=0; ii<4; ++ii) b.bytes[ii] = opusReader[byteMark+ii];
		int packetSize = b.integer;

		byteMark += 4; // Move past the packet size

		Godot::print("Decode packetSize: {0}", packetSize);
		Godot::print("byteMark: {0}", byteMark);
		Godot::print("opusEncoded: {0}", numInputBytes);

		if(packetSize <= 0 || packetSize > 2048)
		{
			Godot::print("Bad packet size, exiting.");
			break;
		}

		// Copy packet into buffer
		for(int ii=0; ii<packetSize; ++ii)
		{
			inBuff[ii] = opusReader[byteMark+ii];
		}
		byteMark += packetSize; // move past the packet

		// If this is the last packet, we will exit when we finish this pass
		if(byteMark >= numInputBytes-5)
		{
			done = true;
		}

		//Godot::print("opus size: {0} byteMark: {1}\n", opusEncoded.size(), byteMark);

		// Decode the current opus packet
		int out_frame_size = opus_decode(decoder, inBuff, packetSize, out, max_frame_size, 0);
		if(out_frame_size < 0)
		{
			Godot::print("decoder failed: {0}", opus_strerror(out_frame_size));
			break;
		}

		// Convert decoded data to little-endian
		for(int i = 0; i < channels * out_frame_size; i++)
		{
			pcmSamples[2 * i] = out[i] & 0xFF;
			pcmSamples[2 * i + 1] = (out[i] >> 8) & 0xFF;
		}

		// Copy PCM samples into output buffer
		for(int ii = 0; ii<out_frame_size; ++ii)
		{
			uint16_t sample = pcmSamples[ii];
			decodedPcm.append(sample);
		}
		/*
		const uint8_t *pcmBytes = reinterpret_cast<uint8_t *>(pcmSamples);
		int numPcmBytes = channels * pcm_channel_size * out_frame_size;
		for(int ii=0; ii<numPcmBytes; ++ii)
		{
			decodedPcm.append(pcmBytes[ii]);
		}
		*/
	}

	delete [] inBuff;
	delete [] pcmSamples;
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
