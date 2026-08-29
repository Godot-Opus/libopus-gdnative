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

OpusDecoderNode::~OpusDecoderNode()
{
	lock_guard<mutex> guard(decoder_mutex);
	_free_state();
}

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
	delete[] outBuff;
	outBuff = new opus_int16[outBuffSize];
	delete[] floatOutBuff;
	floatOutBuff = new float[outBuffSize];

	int err;
	decoder = opus_decoder_create(sample_rate, channels, &err);
	if(err < 0 || decoder == nullptr)
	{
		WARN_PRINT(vformat("failed to create decoder: %s", opus_strerror(err)));
		decoder = nullptr;
	}
}

// Must be called with decoder_mutex held
void OpusDecoderNode::_free_state()
{
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

void OpusDecoderNode::_exit_tree()
{
	lock_guard <mutex> guard(decoder_mutex);
	_free_state();
}

void OpusDecoderNode::set_sample_rate(int p_sample_rate)
{
	lock_guard<mutex> guard(decoder_mutex);

	if(p_sample_rate != 8000 && p_sample_rate != 12000 && p_sample_rate != 16000
		&& p_sample_rate != 24000 && p_sample_rate != 48000)
	{
		WARN_PRINT("Opus supports sample rates of 8000, 12000, 16000, 24000 or 48000 Hz");
		return;
	}

	if(p_sample_rate == sample_rate) return;

	sample_rate = p_sample_rate;
	// State is recreated with the new configuration on the next call
	_free_state();
}

int OpusDecoderNode::get_sample_rate() const
{
	return sample_rate;
}

void OpusDecoderNode::set_channels(int p_channels)
{
	lock_guard<mutex> guard(decoder_mutex);

	if(p_channels != 1 && p_channels != 2)
	{
		WARN_PRINT("Opus supports 1 or 2 channels");
		return;
	}

	if(p_channels == channels) return;

	channels = p_channels;
	_free_state();
}

int OpusDecoderNode::get_channels() const
{
	return channels;
}

PackedByteArray OpusDecoderNode::decode(const PackedByteArray &opusEncoded)
{
	lock_guard<mutex> guard(decoder_mutex);
	_init_state();

	PackedByteArray decodedPcm;

	if(decoder == nullptr)
	{
		WARN_PRINT("Opus Decoder: no decoder state");
		return decodedPcm;
	}

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

	// Each pass needs a 4 byte header plus at least 1 data byte. Streams from older
	// encoder versions carry one trailing garbage byte, which this also skips.
	while(numInputBytes - byteMark >= 5)
	{
		// Clear the buffers
		memset(outBuff, 0, outBuffSize * sizeof(opus_int16));

		// Parse out packet size
		Bytes4 b{0};
		for(int ii = 0; ii < 4; ++ii) b.bytes[ii] = compressedBytes[byteMark + ii];
		const int packetSize = b.integer;

		byteMark += 4; // Move past the packet size

		// Very unintelligent sanity check to make sure our packet size header wasn't corrupt
		if(packetSize <= 0 || packetSize > MAX_PACKET_SIZE || packetSize > numInputBytes - byteMark)
		{
			WARN_PRINT("Bad packet size, exiting.");
			break;
		}

		// Get pointer to current packet
		const unsigned char *inData = &compressedBytes[byteMark];

		byteMark += packetSize; // move past the packet

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
	if(decodedPcm.size() > outByteMark)
	{
		decodedPcm.resize(outByteMark);
	}

	return decodedPcm;
}

// Must be called with decoder_mutex held. data == nullptr runs packet loss concealment
PackedVector2Array OpusDecoderNode::_decode_float_packet(const unsigned char *data, int dataSize)
{
	PackedVector2Array frames;

	// A real packet may hold up to 120ms, but concealment should only guess one frame
	const int maxFrames = data != nullptr ? max_frame_size : frame_size;

	int out_frame_size = opus_decode_float(decoder, data, dataSize, floatOutBuff, maxFrames, 0);
	if(out_frame_size < 0)
	{
		WARN_PRINT(vformat("decoder failed: %s", opus_strerror(out_frame_size)));
		return frames;
	}

	frames.resize(out_frame_size);
	Vector2 *out = frames.ptrw();
	if(channels == 1)
	{
		// Duplicate the mono signal into both channels
		for(int ii = 0; ii < out_frame_size; ++ii)
		{
			out[ii] = Vector2(floatOutBuff[ii], floatOutBuff[ii]);
		}
	}
	else
	{
		for(int ii = 0; ii < out_frame_size; ++ii)
		{
			out[ii] = Vector2(floatOutBuff[ii * 2], floatOutBuff[ii * 2 + 1]);
		}
	}

	return frames;
}

PackedVector2Array OpusDecoderNode::decode_frame(const PackedByteArray &packet)
{
	lock_guard<mutex> guard(decoder_mutex);
	_init_state();

	if(decoder == nullptr)
	{
		return PackedVector2Array();
	}

	const int packetSize = packet.size();
	if(packetSize <= 0 || packetSize > MAX_PACKET_SIZE)
	{
		WARN_PRINT("Opus Decoder: bad packet size");
		return PackedVector2Array();
	}

	return _decode_float_packet(packet.ptr(), packetSize);
}

PackedVector2Array OpusDecoderNode::decode_dropped()
{
	lock_guard<mutex> guard(decoder_mutex);
	_init_state();

	if(decoder == nullptr)
	{
		return PackedVector2Array();
	}

	return _decode_float_packet(nullptr, 0);
}

void OpusDecoderNode::reset_stream()
{
	lock_guard<mutex> guard(decoder_mutex);
	_init_state();

	if(decoder == nullptr)
	{
		return;
	}

	int err = opus_decoder_ctl(decoder, OPUS_RESET_STATE);
	if(err < 0)
	{
		WARN_PRINT(vformat("failed to reset decoder: %s", opus_strerror(err)));
	}
}


void OpusDecoderNode::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("decode", "opus_encoded"), &OpusDecoderNode::decode);

	ClassDB::bind_method(D_METHOD("set_sample_rate", "sample_rate"), &OpusDecoderNode::set_sample_rate);
	ClassDB::bind_method(D_METHOD("get_sample_rate"), &OpusDecoderNode::get_sample_rate);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sample_rate", PROPERTY_HINT_ENUM, "8000:8000,12000:12000,16000:16000,24000:24000,48000:48000"), "set_sample_rate", "get_sample_rate");

	ClassDB::bind_method(D_METHOD("set_channels", "channels"), &OpusDecoderNode::set_channels);
	ClassDB::bind_method(D_METHOD("get_channels"), &OpusDecoderNode::get_channels);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "channels", PROPERTY_HINT_ENUM, "Mono:1,Stereo:2"), "set_channels", "get_channels");

	ClassDB::bind_method(D_METHOD("decode_frame", "packet"), &OpusDecoderNode::decode_frame);
	ClassDB::bind_method(D_METHOD("decode_dropped"), &OpusDecoderNode::decode_dropped);
	ClassDB::bind_method(D_METHOD("reset_stream"), &OpusDecoderNode::reset_stream);
}
