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
	application = OPUS_APPLICATION_VOIP;
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

	/*
	err = opus_encoder_ctl(encoder, OPUS_SET_FORCE_CHANNELS(1));
	if (err<0)
	{
		Godot::print(String().format("failed to set force mono: {0}\n", opus_strerror(err)));
	}
	*/
}

void OpusEncoderNode::_exit_tree()
{
	if(encoder != nullptr)
	{
		opus_encoder_destroy(encoder);
		encoder = nullptr;
	}
}


inline double lerp(double v0, double v1, double t) {
	return (1.0 - t) * v0 + t * v1;
}

/*
PoolByteArray OpusEncoderNode::resample_441kh_48kh(const PoolByteArray &rawPcm)
{
	const float ratio = 160.0f/147.0f;
	const int numRawBytes = rawPcm.size();
	const PoolByteArray::Read reader = rawPcm.read();
	const opus_int16 *readSamples = reinterpret_cast<const opus_int16 *>(reader.ptr());

	const int upsampledSize = (int)floor(numRawBytes * ratio);
	const int numWriteSamples = upsampledSize / pcm_channel_size;
	opus_int16 *writableSamples = new opus_int16[numWriteSamples];

	const float mappingRatio = 44100.0f/48000.0f;

	int map44XLast = 0;

	for(int xx=0; xx<numWriteSamples-1; xx+=channels)
	{
		//int map44X = (int)round((float)xx * mappingRatio);
		float smallCord = (float)xx * mappingRatio;
		double intpart;
		double percent = (float)modf(smallCord , &intpart);
		int map44X = (int)intpart;

		if(map44XLast == map44X)
		{
			Godot::print("INTERP {0}", map44X);
			writableSamples[xx] = lerp((double)readSamples[map44XLast], (double)readSamples[map44X], percent);
			//writableSamples[xx] = (opus_int16)(((float)readSamples[map44X] + (float)readSamples[map44XLast]) / 2.0f);
			if (channels==2)
			{
				writableSamples[xx+1] = lerp((double)readSamples[map44XLast+1],(double)readSamples[map44X+1],  percent);
				//writableSamples[xx+1] = (opus_int16)(((float)readSamples[map44X+1] + (float)readSamples[map44XLast+1]) / 2.0f);
			}
		}
		else
		{
			writableSamples[xx] = readSamples[map44X];
			if (channels==2)
			{
				writableSamples[xx+1] = readSamples[map44X+1];
			}
		}

		map44XLast = map44X;
	}

	PoolByteArray upsampled;
	//upsampled.resize(upsampledBytesSize);
	unsigned char *writableSampleBytes = reinterpret_cast<unsigned char *>(writableSamples);
	for(int ii=0; ii < numWriteSamples*pcm_channel_size; ++ii)
	{
		upsampled.append(writableSampleBytes[ii]);
	}

//	for(int ii=0; ii<1000; ++ii)
//	{
//		Godot::print("rdy: {0}", writableSamples[ii]);
//	}

	delete [] writableSamples;

	return upsampled;
}
*/

PoolByteArray OpusEncoderNode::encode(const PoolByteArray &rawPcm)
{
	PoolByteArray encodedBytes;

	//PoolByteArray upsampled = resample_441kh_48kh(rawPcm);

	const int numPcmBytes = rawPcm.size();
	const unsigned char *pcm_bytes = rawPcm.read().ptr();

	const int bytesPerSample = pcm_channel_size * channels;

	opus_int16 *inputSamples = new opus_int16[frame_size * channels];

	// This is the temp buffer we encode to
	unsigned char *outBuff = new unsigned char[sizeof(unsigned char) * MAX_PACKET_SIZE];

	const int availableSamples = numPcmBytes / bytesPerSample;

	int remainingSamples = availableSamples;

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
		}

		markPos += (curFrameSize * channels * pcm_channel_size);

		// Encode the frame.
		int opusPacketSize = opus_encode(encoder, inputSamples, frame_size, outBuff, MAX_PACKET_SIZE);
		if(opusPacketSize < 0)
		{
			Godot::print("encode failed: {0}", opus_strerror(opusPacketSize));
			break;
		}

		// Prepend the frame size
		Bytes4 b{opusPacketSize};
		for(unsigned char byte : b.bytes) encodedBytes.append(byte);

		// Resize to fit the new frame
		int initialSize = encodedBytes.size();
		encodedBytes.resize(initialSize + opusPacketSize);

		// Copy the new data into the output array
		uint8_t *pbaData = encodedBytes.write().ptr();
		uint8_t *targetArea = &(pbaData[initialSize]);
		memcpy(targetArea, outBuff, opusPacketSize);

		// Record that we've processed how ever many frames we processed
		remainingSamples -= curFrameSize;
	}

	delete[] inputSamples;
	delete[] outBuff;

	return encodedBytes;
}

void OpusEncoderNode::_register_methods()
{
	register_property<OpusEncoderNode, int>("bit_rate", &OpusEncoderNode::bit_rate, DEFAULT_BITRATE);

	register_method("_init", &OpusEncoderNode::_init);
	register_method("_ready", &OpusEncoderNode::_ready);
	register_method("_exit_tree", &OpusEncoderNode::_exit_tree);
	//register_method("resample_441kh_48kh", &OpusEncoderNode::resample_441kh_48kh);
	register_method("encode", &OpusEncoderNode::encode);
}