// test_r_renderscale.cpp - render size and resampling of r_renderScale
//
// Links the real engine source (src/renderer/tr_renderscale.cpp), which has
// no engine dependency.

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include "renderer/tr_renderscale.h"

namespace {

struct Size {
	int width, height;
};

Size Scaled( int windowWidth, int windowHeight, float scale, int maxSize ) {
	Size s = { -1, -1 };
	R_RenderScaleSize( windowWidth, windowHeight, scale, maxSize, &s.width, &s.height );
	return s;
}

// One axis of the tap program shrinking srcSize texels to dstSize: each
// destination texel averages two bilinear taps at +-offset around its
// center, clamped inside the source like the program does. Returns the
// total weight each source texel gets.
std::vector<double> TapWeights( int srcSize, int dstSize, double offset ) {
	const double		step = (double)srcSize / dstSize;
	std::vector<double>	weights( srcSize, 0.0 );

	for ( int i = 0; i < dstSize; i++ ) {
		const double center = ( i + 0.5 ) * step;
		const double taps[2] = { center - offset, center + offset };

		for ( int t = 0; t < 2; t++ ) {
			const double	p = std::min( std::max( taps[t], 0.5 ), srcSize - 0.5 );
			const int		k = (int)std::floor( p - 0.5 );
			const double	f = p - 0.5 - k;

			weights[k] += ( 1.0 - f ) / 2;
			if ( k + 1 < srcSize ) {
				weights[k + 1] += f / 2;
			}
		}
	}
	return weights;
}

// largest over smallest weight, away from the clamped borders
double Unevenness( const std::vector<double> &weights ) {
	const std::vector<double> inner( weights.begin() + 4, weights.end() - 4 );
	const double lowest = *std::min_element( inner.begin(), inner.end() );

	return lowest > 0.0 ? *std::max_element( inner.begin(), inner.end() ) / lowest
		: std::numeric_limits<double>::infinity();
}

} // namespace

TEST(RenderScaleValue, KeepsTheRange) {
	EXPECT_FLOAT_EQ( R_RenderScaleValue( 1.0f ), 1.0f );
	EXPECT_FLOAT_EQ( R_RenderScaleValue( 0.5f ), 0.5f );
	EXPECT_FLOAT_EQ( R_RenderScaleValue( 1.5f ), 1.5f );
	EXPECT_FLOAT_EQ( R_RenderScaleValue( 2.0f ), 2.0f );
}

TEST(RenderScaleValue, ClampsOutOfRange) {
	EXPECT_FLOAT_EQ( R_RenderScaleValue( 0.25f ), RENDERSCALE_MIN );
	EXPECT_FLOAT_EQ( R_RenderScaleValue( 0.01f ), RENDERSCALE_MIN );
	EXPECT_FLOAT_EQ( R_RenderScaleValue( 3.0f ), RENDERSCALE_MAX );
	EXPECT_FLOAT_EQ( R_RenderScaleValue( 1e30f ), RENDERSCALE_MAX );
}

TEST(RenderScaleValue, ZeroNegativeAndNaNAreOff) {
	EXPECT_FLOAT_EQ( R_RenderScaleValue( 0.0f ), 1.0f );
	EXPECT_FLOAT_EQ( R_RenderScaleValue( -1.0f ), 1.0f );
	EXPECT_FLOAT_EQ( R_RenderScaleValue( std::numeric_limits<float>::quiet_NaN() ), 1.0f );
}

TEST(RenderScaleSize, ScaleOneIsTheWindow) {
	const Size s = Scaled( 1280, 720, 1.0f, 16384 );
	EXPECT_EQ( s.width, 1280 );
	EXPECT_EQ( s.height, 720 );

	const Size odd = Scaled( 1279, 719, 1.0f, 0 );
	EXPECT_EQ( odd.width, 1279 );
	EXPECT_EQ( odd.height, 719 );
}

TEST(RenderScaleSize, ScalesBothSides) {
	const Size half = Scaled( 1280, 720, 0.5f, 16384 );
	EXPECT_EQ( half.width, 640 );
	EXPECT_EQ( half.height, 360 );

	const Size oneHalf = Scaled( 1280, 720, 1.5f, 16384 );
	EXPECT_EQ( oneHalf.width, 1920 );
	EXPECT_EQ( oneHalf.height, 1080 );

	const Size twice = Scaled( 1280, 720, 2.0f, 16384 );
	EXPECT_EQ( twice.width, 2560 );
	EXPECT_EQ( twice.height, 1440 );
}

TEST(RenderScaleSize, RoundsToNearest) {
	const Size s = Scaled( 1366, 768, 1.5f, 0 );
	EXPECT_EQ( s.width, 2049 );
	EXPECT_EQ( s.height, 1152 );

	const Size half = Scaled( 1279, 719, 0.5f, 0 );
	EXPECT_EQ( half.width, 640 );		// 639.5
	EXPECT_EQ( half.height, 360 );		// 359.5
}

TEST(RenderScaleSize, LowersTheScaleToFitTheLimit) {
	// a 4K window at scale 2 on a GL that renders up to 4096
	const Size s = Scaled( 3840, 2160, 2.0f, 4096 );
	EXPECT_EQ( s.width, 4096 );
	EXPECT_EQ( s.height, 2304 );		// the same scale, 16:9 kept

	// limited by the taller side
	const Size tall = Scaled( 1080, 1920, 2.0f, 2048 );
	EXPECT_EQ( tall.height, 2048 );
	EXPECT_EQ( tall.width, 1152 );

	// already past the limit at scale 1: still never above it
	const Size big = Scaled( 8000, 100, 1.0f, 4096 );
	EXPECT_EQ( big.width, 4096 );
	EXPECT_EQ( big.height, 51 );
}

TEST(RenderScaleSize, LimitDoesNotChangeSmallerSizes) {
	const Size s = Scaled( 1920, 1080, 2.0f, 4096 );
	EXPECT_EQ( s.width, 3840 );
	EXPECT_EQ( s.height, 2160 );

	const Size exact = Scaled( 2048, 1024, 2.0f, 4096 );
	EXPECT_EQ( exact.width, 4096 );
	EXPECT_EQ( exact.height, 2048 );
}

TEST(RenderScaleSize, NeverBelowOnePixel) {
	const Size s = Scaled( 1, 1, 0.5f, 16384 );
	EXPECT_EQ( s.width, 1 );
	EXPECT_EQ( s.height, 1 );
}

TEST(RenderScaleSize, EmptyWindowPassesThrough) {
	const Size zero = Scaled( 0, 0, 2.0f, 16384 );
	EXPECT_EQ( zero.width, 0 );
	EXPECT_EQ( zero.height, 0 );

	const Size flat = Scaled( 1280, 0, 2.0f, 16384 );
	EXPECT_EQ( flat.width, 1280 );
	EXPECT_EQ( flat.height, 0 );
}

TEST(RenderScaleSize, CommonWindowsAtEveryScale) {
	static const int windows[][2] = { { 640, 480 }, { 1280, 720 }, { 1920, 1080 }, { 2560, 1440 }, { 3840, 2160 }, { 1366, 768 } };
	static const float scales[] = { 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f };

	for ( size_t w = 0; w < sizeof( windows ) / sizeof( windows[0] ); w++ ) {
		for ( size_t k = 0; k < sizeof( scales ) / sizeof( scales[0] ); k++ ) {
			const Size s = Scaled( windows[w][0], windows[w][1], scales[k], 16384 );
			// each side within half a pixel of the exact size
			EXPECT_LE( std::fabs( s.width - windows[w][0] * scales[k] ), 0.5f );
			EXPECT_LE( std::fabs( s.height - windows[w][1] * scales[k] ), 0.5f );
		}
	}
}

TEST(RenderScaleShownSide, WithoutScaleItIsTheSide) {
	EXPECT_EQ( R_RenderScaleShownSide( 1280, 1280, 1280 ), 1280 );
	EXPECT_EQ( R_RenderScaleShownSide( 333, 1280, 1280 ), 333 );
	EXPECT_EQ( R_RenderScaleShownSide( 1279, 1279, 1279 ), 1279 );
	// no window size known
	EXPECT_EQ( R_RenderScaleShownSide( 1280, 2560, 0 ), 1280 );
	EXPECT_EQ( R_RenderScaleShownSide( 1280, 0, 1280 ), 1280 );
}

TEST(RenderScaleShownSide, BringsTheFrameToTheWindow) {
	// whole frames of 1280x720 at 2, 1.5 and 0.5, and 1366x768 at 1.5
	EXPECT_EQ( R_RenderScaleShownSide( 2560, 2560, 1280 ), 1280 );
	EXPECT_EQ( R_RenderScaleShownSide( 1080, 1080, 720 ), 720 );
	EXPECT_EQ( R_RenderScaleShownSide( 640, 640, 1280 ), 1280 );
	EXPECT_EQ( R_RenderScaleShownSide( 2049, 2049, 1366 ), 1366 );
	// a view of part of the frame, rounded to the nearest pixel
	EXPECT_EQ( R_RenderScaleShownSide( 1280, 2560, 1280 ), 640 );
	EXPECT_EQ( R_RenderScaleShownSide( 1001, 2560, 1280 ), 501 );
	// never empty
	EXPECT_EQ( R_RenderScaleShownSide( 1, 2560, 1280 ), 1 );
}

TEST(RenderScaleBoxOffset, TwoIsTheExactBox) {
	EXPECT_FLOAT_EQ( R_RenderScaleBoxOffset( 2.0f ), 0.5f );
	EXPECT_FLOAT_EQ( R_RenderScaleBoxOffset( 1.0f ), 0.25f );

	// the taps fall on the texel centers: each source texel counts once
	const std::vector<double> w = TapWeights( 128, 64, R_RenderScaleBoxOffset( 2.0f ) );
	for ( size_t i = 0; i < w.size(); i++ ) {
		EXPECT_DOUBLE_EQ( w[i], 0.5 ) << "texel " << i;
	}
}

TEST(RenderScaleBoxOffset, EvenBetweenOneAndTwo) {
	// from scale 1 to 2 (shown frames, the bloom's first level above scale
	// 1) and from 1 to 2 texels per level texel (the bloom below scale 1)
	for ( int dst = 64; dst <= 65; dst++ ) {
		for ( int src = dst; src <= 2 * dst; src++ ) {
			const double ratio = (double)src / dst;
			const double unevenness = Unevenness( TapWeights( src, dst, R_RenderScaleBoxOffset( (float)ratio ) ) );
			EXPECT_LE( unevenness, 1.3 ) << src << " to " << dst;
		}
	}
}

TEST(RenderScaleBoxOffset, OneBilinearTapWouldNotBe) {
	// why four taps: with one bilinear tap per window pixel, some render
	// pixels count 3.5 times as much as others at scale 1.75 (1.5 times at
	// 1.5), so thin lines shimmer as they move
	EXPECT_GE( Unevenness( TapWeights( 112, 64, 0.0 ) ), 3.0 );
	EXPECT_LE( Unevenness( TapWeights( 112, 64, R_RenderScaleBoxOffset( 1.75f ) ) ), 1.1 );
	EXPECT_GE( Unevenness( TapWeights( 96, 64, 0.0 ) ), 1.45 );
	EXPECT_LE( Unevenness( TapWeights( 96, 64, R_RenderScaleBoxOffset( 1.5f ) ) ), 1.15 );
}
