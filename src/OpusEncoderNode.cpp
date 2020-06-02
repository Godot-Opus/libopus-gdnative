//
// Created by Adam on 5/30/2020.
//

#include "OpusEncoderNode.h"
#include "Values.h"

using namespace std;
using namespace opus;
using namespace godot;

OpusEncoderNode::OpusEncoderNode() : Node()
{

}

OpusEncoderNode::~OpusEncoderNode()
{

}

void OpusEncoderNode::_init()
{
	application = OPUS_APPLICATION_AUDIO;
	sample_rate = DEFAULT_SAMPLE_RATE;
	bit_rate = DEFAULT_BITRATE;
	channels = DEFAULT_CHANNELS;
	frame_size = 0;

	pcm_channel_size = sizeof(opus_uint16);
}

void OpusEncoderNode::_ready()
{
	int err;

	frame_size = sample_rate / 50;
	max_frame_size = frame_size * 6;
	Godot::print("sample_rate: {0} channels: {1}", sample_rate, channels);
	/*Create a new encoder state */
	encoder = opus_encoder_create(sample_rate, channels, application, &err);
	if(err < 0)
	{
		Godot::print(String().format("failed to create an encoder: %s\n", opus_strerror(err)));
	}

	err = opus_encoder_ctl(encoder, OPUS_SET_BITRATE(bit_rate));
	if (err<0)
	{
		Godot::print(String().format("failed to set bitrate: {0}\n", opus_strerror(err)));
	}

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

PoolByteArray OpusEncoderNode::resample_441kh_48kh(const PoolByteArray &rawPcm)
{
	const float ratio = 160.0f/147.0f;
	const int numRawBytes = rawPcm.size();
	const PoolByteArray::Read reader = rawPcm.read();
	const opus_int16 *readSamples = reinterpret_cast<const opus_int16 *>(reader.ptr());
	const int numReadSamples = rawPcm.size() / 2;

	const int upsampledSize = (int)floor(numRawBytes * ratio);
	const int numWriteSamples = upsampledSize / pcm_channel_size;
	opus_int16 *writableSamples = new opus_int16[numWriteSamples];

	Godot::print("numRawBytes: {0}", numRawBytes);
	Godot::print("upsampledBytesSize: {0}", upsampledSize);
//	PoolByteArray upsampled;
//	upsampled.resize(newSize);
//	PoolByteArray::Write writable = upsampled.write();
//	uint16_t *writableSamples = reinterpret_cast<uint16_t *>(writable.ptr());
//	const int numWriteSamples = newSize / 2;

	const float mappingRatio = 44100.0f/48000.0f;

//	for(int ii=0; ii<1000; ++ii)
//	{
//		Godot::print("orig: {0}", readSamples[ii]);
//	}

	/*
	for(int ii=0; ii<numReadSamples; ++ii)
	{
		Godot::print("org sample: {0}", readSamples[ii]);
	}
	*/

	int map44XLast = 0;
	//for(int xx=0; xx<numWriteSamples; xx+=channels)
	for(int xx=0; xx<numWriteSamples-1; xx+=channels)
	{
		int map44X = (int)round((float)xx * mappingRatio);
		//Godot::print("numReadSamples: {0}", numReadSamples);
		//Godot::print("map44X: {0} map44XLast: {1}", map44X, map44XLast);
		if(map44XLast == map44X)
		{
			writableSamples[xx] = (uint16_t)(((int)readSamples[map44X] + (int)readSamples[map44XLast]) / 2);
			//Godot::print("up sample INTER: {0}", writableSamples[xx]);
			if (channels==2)
			{
				writableSamples[xx+1] = (uint16_t)(((int)readSamples[map44X+1] + (int)readSamples[map44XLast+1]) / 2);
				//Godot::print("up sample INTER2: {0}", writableSamples[xx+1]);
			}
		}
		else
		{
			//Godot::print("up read: {0}", readSamples[map44X]);
			writableSamples[xx] = (uint16_t)readSamples[map44X];
			//Godot::print("up sample: {0}", writableSamples[xx]);
			if (channels==2)
			{
				//Godot::print("up read: {0}", readSamples[map44X+1]);
				writableSamples[xx+1] = (uint16_t)readSamples[map44X+1];
				//Godot::print("up sample2: {0}", (uint16_t)writableSamples[xx+1]);
			}
		}

		map44XLast = map44X;
	}

//	for(int ii=0; ii<1000; ++ii)
//	{
//		Godot::print("up: {0}", writableSamples[ii]);
//	}



	//uint8_t *writableSampleBytes = reinterpret_cast<uint8_t *>(writableSamples);
	PoolByteArray upsampled;
	//upsampled.resize(upsampledBytesSize);
	PoolByteArray::Write writer = upsampled.write();
	for(int ii=0; ii < numWriteSamples; ++ii)
	{
		//Godot::print("up sample: {0}", writableSampleBytes[ii]);
		opus_int16 sample = writableSamples[ii];
		//Godot::print("UP sample: {0}", sample);
		Bytes2 b{};
		b.integer = sample;
		//uint8_t* bytes = reinterpret_cast<uint8_t*>(&sample);
		//uint8_t first = (sample >> (0 * 8)) & 0xFF;
		//uint8_t second = (sample >> (1 * 8)) & 0xFF;

		upsampled.append(b.bytes[0]);
		upsampled.append(b.bytes[1]);

		//upsampled.append(writableSampleBytes[ii]);
		//writer[ii] = writableSampleBytes[ii];
	}

//	for(int ii=0; ii<1000; ++ii)
//	{
//		Godot::print("rdy: {0}", writableSamples[ii]);
//	}

	delete [] writableSamples;


	return upsampled;
}

PoolByteArray OpusEncoderNode::encode(const PoolByteArray &rawPcm)
{
	PoolByteArray encodedBytes;

	//PoolByteArray upsampled = resample_441kh_48kh(rawPcm);
	PoolByteArray upsampled = rawPcm;

	const int numPcmBytes = upsampled.size();
	const unsigned char *pcm_bytes = upsampled.read().ptr();

	const opus_int16 *pcm_samples = reinterpret_cast<const opus_int16 *>(pcm_bytes);
	for(int ii=0; ii<500; ++ii)
	{
		//Godot::print("rdy : {0}", pcm_samples[ii]);
		Godot::print("rdy: {0}", (pcm_bytes[2 * ii + 1] << 8 | pcm_bytes[2 * ii]));
	}

	Godot::print("numPcmBytes: {0}", numPcmBytes);

	const int bytesPerSample = pcm_channel_size * channels;
	Godot::print("bytes per sample: {0}", bytesPerSample);

	//const int frameSizeBytes = frame_size *  bytesPerSample;
	opus_int16 *inputSamples = new opus_int16[frame_size * channels];

	// This is the temp buffer we encode to
	unsigned char *outBuff = new unsigned char[sizeof(unsigned char) * MAX_PACKET_SIZE];

	opus_int16 *out2 = new int16_t[max_frame_size*channels];
	unsigned char *outBuff3 = new unsigned char[sizeof(unsigned char) * max_frame_size * channels * pcm_channel_size];

	const int availableSamples = numPcmBytes / bytesPerSample;
	//const int availableFrames = availableSamples / frame_size;

	int remainingSamples = availableSamples;

	Godot::print("frame_size: {0}", frame_size);
	Godot::print("availableSamples: {0}", availableSamples);

	int markPos = 0;

	bool done = false;
	while(!done)
	{
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

		// Remember: input_bytes elements are 2 bytes wide, pcm_bytes are 1 byte wide.


		// Convert from little-endian ordering. (I think it does nothing if you are already on a little endian system)
		for(int ii = 0; ii < (curFrameSize * channels); ii++)
		{
			inputSamples[ii] = pcm_bytes[markPos + (2 * ii + 1)] << 8 | pcm_bytes[markPos + (2 * ii)];
			//Godot::print("in sample: {0}", inputSamples[ii]);
		}

		markPos += (curFrameSize * channels * pcm_channel_size);

//		Godot::print("--------------------");
//		for(int ii=0; ii<10; ++ii)
//		{
//			Godot::print("in: {0}", inputSamples[ii]);
//		}

//		for(int ii=0; ii<500; ++ii)
//		{
//			Godot::print("lst: {0}", inputSamples[ii]);
//		}

		// Encode the frame.
		int opusPacketSize = opus_encode(encoder, inputSamples, frame_size, outBuff, MAX_PACKET_SIZE);
		if(opusPacketSize < 0)
		{
			Godot::print("encode failed: {0}", opus_strerror(opusPacketSize));
			break;
		}

//		int out_frame_size = opus_decode(decoder, outBuff, opusPacketSize, out2, max_frame_size, 0);
//		if(out_frame_size < 0)
//		{
//			Godot::print("decoder failed: {0}", opus_strerror(out_frame_size));
//			break;
//		}

		//Godot::print("out_frame_size: {0}", out_frame_size);

		// Convert to little-endian ordering.
//		for(int i=0;i<channels*out_frame_size;i++)
//		{
//			outBuff3[2*i]=out2[i]&0xFF;
//			outBuff3[2*i+1]=(out2[i]>>8)&0xFF;
//		}
//
//		for(int ii=0; ii<10; ++ii)
//		{
//			Godot::print("out: {0}", out2[ii]);
//		}
//		Godot::print("--------------------");

		//unsigned char *x = reinterpret_cast<unsigned char *>(out2);
//		for(int ii = 0; ii < out_frame_size*channels*pcm_channel_size; ++ii)
//		{
//			encodedBytes.append(outBuff3[ii]);
//		}


		// Prepend the frame size
		//Godot::print("Encode packetSize: {0}", opusPacketSize);
		Bytes4 b{opusPacketSize};
		for(unsigned char byte : b.bytes) encodedBytes.append(byte);

		// $TODO: Need to pre-allocate the extra space in `encodedBytes`
		// Copy the newly encoded bytes into our output
		for(int ii = 0; ii < opusPacketSize; ++ii)
		{
			encodedBytes.append(outBuff[ii]);
		}


		// Record that we've processed how ever many frames we processed
		remainingSamples -= curFrameSize;
		//Godot::print("remainingSamples: {0}", remainingSamples);
	}

	delete[] inputSamples;
	delete[] outBuff;

	Godot::print("Total Encoded size: {0}", encodedBytes.size());

	return encodedBytes;
}

void OpusEncoderNode::_register_methods()
{
	register_property<OpusEncoderNode, int>("application", &OpusEncoderNode::application, OPUS_APPLICATION_VOIP);
	register_property<OpusEncoderNode, int>("sample_rate", &OpusEncoderNode::sample_rate, DEFAULT_SAMPLE_RATE);
	//register_property<OpusEncoderNode, int>("bit_rate", &OpusEncoderNode::bit_rate, DEFAULT_BITRATE);
	register_property<OpusEncoderNode, int>("channels", &OpusEncoderNode::channels, DEFAULT_CHANNELS);


	register_method("_init", &OpusEncoderNode::_init);
	register_method("_ready", &OpusEncoderNode::_ready);
	register_method("resample_441kh_48kh", &OpusEncoderNode::resample_441kh_48kh);
	register_method("encode", &OpusEncoderNode::encode);
}