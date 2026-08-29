//
// Created by Adam on 5/30/2020.
//

#ifndef OPUS_GDNATIVE_OPUSDECODERNODE_H
#define OPUS_GDNATIVE_OPUSDECODERNODE_H

#include <mutex>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <opus.h>

namespace opus
{
	class OpusDecoderNode : public godot::Node
	{
		GDCLASS(OpusDecoderNode, godot::Node)

	private:
		int frame_size;
		int max_frame_size;
		OpusDecoder *decoder = nullptr;
		int outBuffSize;
		opus_int16 *outBuff = nullptr;
		float *floatOutBuff = nullptr;

		std::mutex decoder_mutex;

		void _init_state();
		void _free_state();
		godot::PackedVector2Array _decode_float_packet(const unsigned char *data, int dataSize);

		int sample_rate;
		int pcm_channel_size;
		int channels;

	protected:
		static void _bind_methods();

	public:
		OpusDecoderNode();
		~OpusDecoderNode();

		void _ready() override;
		void _exit_tree() override;

		void set_sample_rate(int p_sample_rate);
		int get_sample_rate() const;

		void set_channels(int p_channels);
		int get_channels() const;

		godot::PackedByteArray decode(const godot::PackedByteArray &opusEncoded);

		godot::PackedVector2Array decode_frame(const godot::PackedByteArray &packet);
		godot::PackedVector2Array decode_dropped();
		void reset_stream();
	};
}

#endif //OPUS_GDNATIVE_OPUSDECODERNODE_H
