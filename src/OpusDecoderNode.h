//
// Created by Adam on 5/30/2020.
//

#ifndef OPUS_GDNATIVE_OPUSDECODERNODE_H
#define OPUS_GDNATIVE_OPUSDECODERNODE_H

#include <mutex>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
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

		std::mutex decoder_mutex;

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

		godot::PackedByteArray decode(const godot::PackedByteArray &opusEncoded);
	};
}

#endif //OPUS_GDNATIVE_OPUSDECODERNODE_H
