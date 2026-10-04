// test_cl_wav.cpp - the WAV header of video_mp4's sound track
//
// Links nothing: src/client/cl_wav.h has no engine dependency.

#include <gtest/gtest.h>
#include <string.h>
#include "client/cl_wav.h"

namespace {

uint32_t Get32( const unsigned char *p ) {
	return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

unsigned int Get16( const unsigned char *p ) {
	return p[0] | p[1] << 8;
}

// the RIFF size and the data size of the header for dataBytes of sound
void Sizes( int64_t dataBytes, uint32_t *riff, uint32_t *data ) {
	unsigned char header[WAV_HEADER_SIZE];

	CL_WAVHeader( header, dataBytes, 22050, 2, 16 );
	*riff = Get32( header + 4 );
	*data = Get32( header + 40 );
}

} // namespace

TEST(WAVHeader, Layout) {
	unsigned char header[WAV_HEADER_SIZE];

	CL_WAVHeader( header, 22050 * 4 * 2, 22050, 2, 16 );
	EXPECT_EQ( memcmp( header, "RIFF", 4 ), 0 );
	EXPECT_EQ( Get32( header + 4 ), 36u + 176400u );
	EXPECT_EQ( memcmp( header + 8, "WAVEfmt ", 8 ), 0 );
	EXPECT_EQ( Get32( header + 16 ), 16u );
	EXPECT_EQ( Get16( header + 20 ), 1u );			// PCM
	EXPECT_EQ( Get16( header + 22 ), 2u );
	EXPECT_EQ( Get32( header + 24 ), 22050u );
	EXPECT_EQ( Get32( header + 28 ), 88200u );
	EXPECT_EQ( Get16( header + 32 ), 4u );
	EXPECT_EQ( Get16( header + 34 ), 16u );
	EXPECT_EQ( memcmp( header + 36, "data", 4 ), 0 );
	EXPECT_EQ( Get32( header + 40 ), 176400u );

	CL_WAVHeader( header, 0, 44100, 1, 16 );
	EXPECT_EQ( Get32( header + 4 ), 36u );
	EXPECT_EQ( Get16( header + 22 ), 1u );
	EXPECT_EQ( Get32( header + 28 ), 88200u );
	EXPECT_EQ( Get16( header + 32 ), 2u );
	EXPECT_EQ( Get32( header + 40 ), 0u );
}

TEST(WAVHeader, ExactUpTo4GB) {
	uint32_t riff, data;

	Sizes( 0xFFFFFFFFll - 36, &riff, &data );
	EXPECT_EQ( riff, 0xFFFFFFFFu );
	EXPECT_EQ( data, 0xFFFFFFDBu );
	Sizes( 0x80000000ll, &riff, &data );
	EXPECT_EQ( riff, 0x80000024u );
	EXPECT_EQ( data, 0x80000000u );
}

// ffmpeg reads exactly the data size it is given, unless it is 0xFFFFFFFF:
// a size saturated at 0xFFFFFFFF - 36 cut 6.8 hours of 44.1 kHz sound
TEST(WAVHeader, UnknownSizesPast4GB) {
	const int64_t sizes[] = { 0xFFFFFFFFll - 35, 0xFFFFFFFFll, 0x100000000ll, 0x180000000ll, 1ll << 40 };

	for ( size_t i = 0; i < sizeof( sizes ) / sizeof( sizes[0] ); i++ ) {
		uint32_t riff, data;

		Sizes( sizes[i], &riff, &data );
		EXPECT_EQ( riff, 0xFFFFFFFFu ) << sizes[i];
		EXPECT_EQ( data, 0xFFFFFFFFu ) << sizes[i];
	}
}
