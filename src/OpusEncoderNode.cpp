//
// Created by Adam on 5/30/2020.
//

#include "OpusEncoderNode.h"
#include "Values.h"
#include "Utils.h"

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

	// Set an initial size for the output buffer
	// Ideally this will not need to be resized during most encodings
	// Then at the end we'll resize down to the size needed. Just 2 resizes
	const int defaultAudioLengthGuessSeconds = 5;
	const int packetsPerSecond = 50;
	const int initialOutputSize = (MAX_PACKET_SIZE * packetsPerSecond * defaultAudioLengthGuessSeconds);
	encodedBytes.resize(initialOutputSize);

	const unsigned char *pcm_bytes = rawPcm.read().ptr();

	const int bytesPerSample = pcm_channel_size * channels;
	const int availableSamples = numPcmBytes / bytesPerSample;
	int remainingSamples = availableSamples;

	int inMarkPos = 0;
	int outPos = 0;
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

		// Copy the input samples into our buffer. This is important because opus_encode() wants
		// to read a full frame_size worth of data. If we have less than a full frame at the end, it would
		// read off the end of pcmSamples. Thus we need a zeroed out buffer so it reads into empty data.
		memcpy(inputSamples, &pcm_bytes[inMarkPos], (curFrameSize * channels * pcm_channel_size));

		inMarkPos += (curFrameSize * channels * pcm_channel_size);

		// Encode the frame.
		int opusPacketSize = opus_encode(encoder, inputSamples, frame_size, outBuff, MAX_PACKET_SIZE);
		if(opusPacketSize < 0)
		{
			WARN_PRINT(String("encode failed: {0}!").format(Array::make(opus_strerror(opusPacketSize))));
			break;
		}

		// Prepend the frame size
		ensure_buffer_size(encodedBytes, outPos, 4);
		uint8_t *pbaData = encodedBytes.write().ptr();
		Bytes4 b{opusPacketSize};
		for(int ii=0; ii<4; ++ii) pbaData[outPos+ii] = b.bytes[ii];
		outPos += 4;

		// Copy the new data into the output array
		ensure_buffer_size(encodedBytes, outPos, opusPacketSize);
		pbaData = encodedBytes.write().ptr(); // We have to get this again incase ensure_buffer_size() resized the buffer
		uint8_t *targetArea = &(pbaData[outPos]);
		memcpy(targetArea, outBuff, opusPacketSize);
		outPos += opusPacketSize;

		// Record that we've processed how ever many frames we processed
		remainingSamples -= curFrameSize;
	}

	// Down size our buffer to the required size
	if(encodedBytes.size() > outPos+1)
	{
		encodedBytes.resize(outPos+1);
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