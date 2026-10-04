// test_cl_videoclock.cpp - fixed time steps of video recording
//
// Links nothing: src/client/cl_videoclock.h has no engine dependency.

#include <gtest/gtest.h>
#include "client/cl_videoclock.h"

namespace {

// game time of n engine frames at fps engine frames per second and timescale,
// checking every frame
void ExpectSteadyTime( int fps, double timescale, int n, int minMsec, int maxMsec ) {
	double	overflow = 0.0;
	long	total = 0;

	for ( int i = 0; i < n; i++ ) {
		const int msec = CL_VideoFrameMsec( 1000.0 / fps * timescale, &overflow );

		ASSERT_GE( msec, minMsec ) << "frame " << i;
		ASSERT_LE( msec, maxMsec ) << "frame " << i;
		ASSERT_GE( overflow, 0.0 );
		ASSERT_LT( overflow, 1.0 );
		total += msec;
	}
	// nothing lost: the frames add up to the video's time
	EXPECT_NEAR( total + overflow, n * 1000.0 / fps * timescale, 1e-6 );
}

} // namespace

TEST(VideoFrameMsec, WholeMillisecondsAt30fps) {
	double		overflow = 0.0;
	const int	expected[] = { 33, 33, 34, 33, 33, 34 };

	for ( int i = 0; i < 6; i++ ) {
		EXPECT_EQ( CL_VideoFrameMsec( 1000.0 / 30, &overflow ), expected[i] );
	}
	ExpectSteadyTime( 30, 1.0, 3000, 33, 34 );
}

// 60 fps with 32 blended frames, before the blend was capped: the old step
// forced 0 to 1 and carried -1 ms frames, a quarter of them
TEST(VideoFrameMsec, NeverNegativeAbove1000EngineFps) {
	ExpectSteadyTime( 60 * 32, 1.0, 1920 * 10, 0, 1 );
}

// the movie preset (60 fps, 4 blended frames) in slow motion
TEST(VideoFrameMsec, NeverNegativeInSlowMotion) {
	ExpectSteadyTime( 60 * 4, 0.2, 2400, 0, 1 );
	ExpectSteadyTime( 30, 0.01, 3000, 0, 1 );
	ExpectSteadyTime( 30 * 32, 0.05, 9600, 0, 1 );
}

TEST(VideoFrameMsec, CapBlendKeepsWholeMilliseconds) {
	// the largest blend at each frame rate gives every frame its own time
	const int rates[] = { 24, 25, 30, 50, 60, 120, 144, 240, 500 };

	for ( size_t i = 0; i < sizeof( rates ) / sizeof( rates[0] ); i++ ) {
		const int blend = CL_VideoBlendFrames( 32, rates[i] );
		ExpectSteadyTime( rates[i] * blend, 1.0, rates[i] * blend * 4, 1, 1000 / ( rates[i] * blend ) + 1 );
	}
}

TEST(VideoBlendFrames, AtMost1000EngineFps) {
	EXPECT_EQ( CL_VideoBlendFrames( 32, 60 ), 16 );
	EXPECT_EQ( CL_VideoBlendFrames( 32, 30 ), 32 );
	EXPECT_EQ( CL_VideoBlendFrames( 32, 120 ), 8 );
	EXPECT_EQ( CL_VideoBlendFrames( 4, 60 ), 4 );
	EXPECT_EQ( CL_VideoBlendFrames( 8, 125 ), 8 );
	EXPECT_EQ( CL_VideoBlendFrames( 32, 500 ), 2 );
	EXPECT_EQ( CL_VideoBlendFrames( 32, 501 ), 1 );
	EXPECT_EQ( CL_VideoBlendFrames( 32, 1000 ), 1 );
	EXPECT_EQ( CL_VideoBlendFrames( 32, 2000 ), 1 );
}

TEST(VideoBlendFrames, RangeOfTheCvar) {
	EXPECT_EQ( CL_VideoBlendFrames( 0, 30 ), 1 );
	EXPECT_EQ( CL_VideoBlendFrames( 1, 30 ), 1 );
	EXPECT_EQ( CL_VideoBlendFrames( -5, 30 ), 1 );
	EXPECT_EQ( CL_VideoBlendFrames( 2, 30 ), 2 );
	EXPECT_EQ( CL_VideoBlendFrames( 100, 30 ), 32 );
	for ( int rate = 1; rate <= 1000; rate++ ) {
		for ( int requested = 2; requested <= 40; requested++ ) {
			const int blend = CL_VideoBlendFrames( requested, rate );
			ASSERT_GE( blend, 1 );
			ASSERT_LE( blend, 32 );
			ASSERT_LE( blend, requested );
			ASSERT_TRUE( blend == 1 || rate * blend <= 1000 ) << rate << " fps, " << requested;
		}
	}
}
