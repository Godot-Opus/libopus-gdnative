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

	const int channel_width = 2;
	// $TODO: max_frame_size here in incorrect, that is a PCM value.
	unsigned char *inBuff = new unsigned char[max_frame_size*channels*channel_width];
	unsigned char *outPcmBytes = new unsigned char[max_frame_size*channels*channel_width];
	opus_int16 *pcmSamples = new int16_t[max_frame_size];
	opus_int16 *out = new int16_t[max_frame_size];

	int byteMark = 0;

	Godot::print("Setup complete.");

	int outBytes = 0;

	bool done = false;
	while(!done)
	{
		// Clear the buffers
		memset(inBuff, 0, sizeof(unsigned char)*max_frame_size*channels*channel_width);
		memset(pcmSamples, 0, sizeof(opus_int16)*max_frame_size);
		memset(out, 0, sizeof(opus_int16)*max_frame_size);
		memset(outPcmBytes, 0, sizeof(unsigned char)*max_frame_size*channels*channel_width);

		// Parse out packet size
		//Godot::print("Parsing packet size...");
		Bytes4 b{0};
		for(int ii=0; ii<4; ++ii) b.bytes[ii] = compressedBytes[byteMark+ii];
		int packetSize = b.integer;

		byteMark += 4; // Move past the packet size

		//Godot::print("Decode packetSize: {0}", packetSize);
		//Godot::print("byteMark: {0}", byteMark);
		//Godot::print("opusEncoded: {0}", numInputBytes);

		if(packetSize <= 0 || packetSize > 2048)
		{
			Godot::print("Bad packet size, exiting.");
			break;
		}

		// Copy packet into buffer
		/*
		for(int ii=0; ii<packetSize; ++ii)
		{
			inBuff[ii] = compressedBytes[byteMark+ii];
		}
		*/
		const unsigned char *inData = &compressedBytes[byteMark];

		byteMark += packetSize; // move past the packet

		// If this is the last packet, we will exit when we finish this pass
		if(byteMark >= numInputBytes-5)
		{
			done = true;
		}

		//Godot::print("opus size: {0} byteMark: {1}\n", opusEncoded.size(), byteMark);

		// Decode the current opus packet
		int out_frame_size = opus_decode(decoder, inData, packetSize, out, max_frame_size, 0);
		if(out_frame_size < 0)
		{
			Godot::print("decoder failed: {0}", opus_strerror(out_frame_size));
			break;
		}

		outBytes += out_frame_size * channels * channel_width;

		//Godot::print("out_frame_size: {0}", out_frame_size);

		/*
		// Convert decoded data to little-endian
		for(int i = 0; i < frame_size*channels*channel_width; i++)
		{
			//Godot::print("out sample: {0}", out[i]);
			Bytes2 b2{};
			b2.integer = out[i];

			decodedPcm.append(b2.bytes[0]);
			decodedPcm.append(b2.bytes[1]);

//			outPcmBytes[2 * i] = out[i] & 0xFF;
//			outPcmBytes[2 * i + 1] = (out[i] >> 8) & 0xFF;
		}
		*/

		unsigned char *bytes = reinterpret_cast<unsigned char*>(out);
		for(int ii=0; ii<out_frame_size*channels*channel_width; ++ii)
		{
			decodedPcm.append(bytes[ii]);
		}

		// Copy PCM samples into output buffer
		/*
		for(int ii = 0; ii<out_frame_size*channels; ++ii)
		{
			opus_int16 sample = pcmSamples[ii];
			//Godot::print("out sample: {0}", sample);
			//uint8_t* bytes = reinterpret_cast<uint8_t*>(&sample);
			//uint8_t first = (sample >> (0 * 8)) & 0xFF;
			//uint8_t second = (sample >> (1 * 8)) & 0xFF;
			Bytes2 b2{};
			b2.integer = sample;

			decodedPcm.append(b2.bytes[0]);
			decodedPcm.append(b2.bytes[1]);
		}
		*/
		/*
		const uint8_t *pcmBytes = reinterpret_cast<uint8_t *>(pcmSamples);
		int numPcmBytes = channels * pcm_channel_size * out_frame_size;
		for(int ii=0; ii<numPcmBytes; ++ii)
		{
			decodedPcm.append(pcmBytes[ii]);
		}
		*/
		//Godot::print("pass complete");
	}

	const opus_int16 *samples = reinterpret_cast<const opus_int16*>(decodedPcm.read().ptr());
	for(int ii=0; ii<500; ++ii)
	{
		Godot::print("out: {0}", samples[ii]);
	}

	Godot::print("outBytes: {0}", outBytes);
	//Godot::print("Decoding complete: {0}", decodedPcm.size());

	delete [] inBuff;
	delete [] pcmSamples;
	delete [] out;
	delete [] outPcmBytes;

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
