//
// Created by Adam on 5/31/2020.
//

#ifndef OPUS_GDNATIVE_VALUES_H
#define OPUS_GDNATIVE_VALUES_H

// 1276 is the recomended value from Opus
constexpr int MAX_PACKET_SIZE = (3 * 1276);
constexpr int DEFAULT_SAMPLE_RATE = 48000;
constexpr int DEFAULT_BITRATE = 15000;
constexpr int DEFAULT_CHANNELS = 2;

union Bytes4
{
	opus_int32 integer;
	unsigned char bytes[4];
};

union Bytes2
{
	opus_int16 integer;
	unsigned char bytes[2];
};

#endif //OPUS_GDNATIVE_VALUES_H
