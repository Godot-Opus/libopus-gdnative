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
	lock_guard<mutex> guard(encoder_mutex);

	int err;

	frame_size = sample_rate / 50;
	max_frame_size = frame_size * 6;

	inputSamplesSize = frame_size * channels;
	inputSamples = new opus_int16[inputSamplesSize];

	// Create a new encoder state
	encoder = opus_encoder_create(sample_rate, channels, application, &err);
	if(err < 0)
	{
		WARN_PRINT(String().format("failed to create an encoder: %s\n", opus_strerror(err)));
	}

	err = opus_encoder_ctl(encoder, OPUS_SET_BITRATE(bit_rate));
	if(err < 0)
	{
		WARN_PRINT(String().format("failed to set bitrate: {0}\n", opus_strerror(err)));
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

PoolByteArray OpusEncoderNode::encode(const PoolByteArray rawPcm)
{
	lock_guard<mutex> guard(encoder_mutex);

	PoolByteArray encodedBytes;

	const int numPcmBytes = rawPcm.size();

	if(numPcmBytes <= 0)
	{
		WARN_PRINT(String("Opus Encoder: empty audio buffer, cannot encode nothing!"));
		return encodedBytes;
	}

	const unsigned char *pcm_bytes = rawPcm.read().ptr();

	const int bytesPerSample = pcm_channel_size * channels;
	const int availableSamples = numPcmBytes / bytesPerSample;
	int remainingSamples = availableSamples;

	int markPos = 0;
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

		// Remember: input_bytes elements are 2 bytes wide, pcm_bytes are 1 byte wide.
		const opus_int16 *pcmSamples = reinterpret_cast<const opus_int16 *>(&pcm_bytes[markPos]);
		// Copy the input samples into our buffer. This is important because opus_encode() wants
		// to read a full frame_size worth of data. If we have less than a full frame at the end, it would
		// read off the end of pcmSamples. Thus we need a zeroed out buffer so it reads into empty data.
		memcpy(inputSamples, pcmSamples, (curFrameSize * channels));

		markPos += (curFrameSize * channels * pcm_channel_size);

		// Encode the frame.
		int opusPacketSize = opus_encode(encoder, inputSamples, frame_size, outBuff, MAX_PACKET_SIZE);
		if(opusPacketSize < 0)
		{
			WARN_PRINT(String("encode failed: {0}!").format(Array::make(opus_strerror(opusPacketSize))));
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