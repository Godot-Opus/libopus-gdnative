//
// Created by Adam on 6/8/2020.
//

#ifndef OPUS_GDNATIVE_UTILS_H
#define OPUS_GDNATIVE_UTILS_H

#include <Godot.hpp>

namespace opus
{
	/**
	 * Double the size of the given buffer. This of course requires
	 * A new allocation, copy of contents, and delete of the old memory block.
	 * It's expensive.
	 * @param encodedBytes Buffer who's size is to be doubled
	 */
	void increase_buffer(godot::PoolByteArray &buffer);

	/**
	 * Check the a buffer has space to add `spaceRequired` more bytes
	 * @param buffer The buffer to be checked
	 * @param usage How many bytes have already been used in the buffer
	 * @param spaceRequired How many new bytes require space
	 */
	bool ensure_buffer_size(godot::PoolByteArray &buffer, int usage, int spaceRequired);
}

#endif //OPUS_GDNATIVE_UTILS_H
