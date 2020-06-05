//
// Created by Adam on 5/30/2020.
//

#ifndef OPUS_GDNATIVE_OPUSDECODERNODE_H
#define OPUS_GDNATIVE_OPUSDECODERNODE_H

#include <Godot.hpp>
#include <Node.hpp>
#include <opus.h>

namespace opus
{
	class OpusDecoderNode : public godot::Node
	{
	private:
	GODOT_CLASS(OpusDecoderNode, godot::Node)

		int frame_size;
		int max_frame_size;
		OpusDecoder *decoder = nullptr;
		int outBuffSize;
		opus_int16 *outBuff = nullptr;
	public:
		int sample_rate;
		int pcm_channel_size;
		int channels;

		OpusDecoderNode();
		~OpusDecoderNode();

		void _init();
		void _ready();
		void _exit_tree();

		godot::PoolByteArray decode(const godot::PoolByteArray& opusEncoded);

		static void _register_methods();
	};
}

#endif //OPUS_GDNATIVE_OPUSDECODERNODE_H
