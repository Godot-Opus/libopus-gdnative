//
// Created by Adam on 5/30/2020.
//

#ifndef OPUS_GDNATIVE_OPUS_H
#define OPUS_GDNATIVE_OPUS_H

#include <Godot.hpp>
#include <Node.hpp>
#include <opus.h>

namespace opus
{
	class OpusEncoderNode : public godot::Node
	{
	private:
		GODOT_CLASS(OpusEncoderNode, godot::Node)

		OpusEncoder *encoder = nullptr;

		/**
		 * Size of each PCM frame in number of samples
		 */
		int frame_size;
		int application;
		int sample_rate;
		int pcm_channel_size;
		int channels;
		int max_frame_size;

	public:
		int bit_rate;

		OpusEncoderNode();
		~OpusEncoderNode();

		void _init();
		void _ready();
		void _exit_tree();

		godot::PoolByteArray resample_441kh_48kh(const godot::PoolByteArray &rawPcm);
		godot::PoolByteArray encode(const godot::PoolByteArray& rawPcm);

		static void _register_methods();
	};
}

#endif //OPUS_GDNATIVE_OPUS_H
