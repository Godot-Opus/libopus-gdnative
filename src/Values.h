//
// Created by Adam on 5/31/2020.
//

#ifndef OPUS_GDNATIVE_VALUES_H
#define OPUS_GDNATIVE_VALUES_H

// 1276 is the recomended value from Opus
constexpr int MAX_PACKET_SIZE = (3 * 1276);
constexpr int DEFAULT_SAMPLE_RATE = 48000;
constexpr int DEFAULT_BITRATE = 64000;
constexpr int DEFAULT_CHANNELS = 2;

union Bytes
{
	int integer;
	uint8_t bytes[4];
};


#endif //OPUS_GDNATIVE_VALUES_H
