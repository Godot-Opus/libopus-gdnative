//
// Created by Adam on 5/30/2020.
//

#ifndef OPUS_GDNATIVE_OPUS_H
#define OPUS_GDNATIVE_OPUS_H

#include <mutex>
#include <vector>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <opus.h>
#include "Values.h"

namespace opus
{
	class OpusEncoderNode : public godot::Node
	{
		GDCLASS(OpusEncoderNode, godot::Node)

	private:
		OpusEncoder *encoder = nullptr;
		int inputSamplesSize;
		opus_int16 *inputSamples = nullptr;
		unsigned char outBuff[sizeof(opus_int16) * MAX_PACKET_SIZE];

		/**
		 * Interleaved stereo float samples accumulated for the streaming path
		 */
		std::vector<float> streamBuffer;

		std::mutex encoder_mutex;

		void _init_state();
		void _free_state();

		/**
		 * Size of each PCM frame in number of samples
		 */
		int frame_size;
		int application;
		int sample_rate;
		int pcm_channel_size;
		int channels;
		int max_frame_size;
		int bit_rate;

	protected:
		static void _bind_methods();

	public:
		OpusEncoderNode();
		~OpusEncoderNode();

		void _ready() override;
		void _exit_tree() override;

		void set_bit_rate(int p_bit_rate);
		int get_bit_rate() const;

		void set_sample_rate(int p_sample_rate);
		int get_sample_rate() const;

		void set_channels(int p_channels);
		int get_channels() const;

		void set_application(int p_application);
		int get_application() const;

		godot::PackedByteArray encode(const godot::PackedByteArray &rawPcm);

		void push_audio(const godot::PackedVector2Array &frames);
		bool has_packet();
		godot::PackedByteArray pop_packet();
		void reset_stream();
	};
}

#endif //OPUS_GDNATIVE_OPUS_H
