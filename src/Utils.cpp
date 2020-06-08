//
// Created by Adam on 6/8/2020.
//

#include "Utils.h"

using namespace godot;

void opus::increase_buffer(godot::PoolByteArray &buffer)
{
	buffer.resize(buffer.size() * 2);
}

bool opus::ensure_buffer_size(PoolByteArray &buffer, int usage, int spaceRequired)
{
	bool wasResized;
	if((buffer.size() - usage) < spaceRequired)
	{
		opus::increase_buffer(buffer);
		wasResized = true;
	}
	else
	{
		wasResized = false;
	}

	return wasResized;
}