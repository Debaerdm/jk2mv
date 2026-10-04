// test_r_coverage.cpp - alpha of the images drawn with alpha to coverage
//
// Links the real engine source (src/renderer/tr_coverage.cpp), which has no
// engine dependency.

#include <gtest/gtest.h>
#include <vector>
#include "renderer/tr_coverage.h"

namespace {

typedef std::vector<unsigned char> Pixels;	// RGBA

// a cut-out like a grass texture: thin diagonal blades with soft edges
Pixels Blades( int size ) {
	Pixels p( size * size * 4 );
	for ( int y = 0; y < size; y++ ) {
		for ( int x = 0; x < size; x++ ) {
			unsigned char *t = &p[( y * size + x ) * 4];
			const int d = ( x + 2 * y ) % 12;	// across the blades
			t[0] = (unsigned char)( 40 + x );
			t[1] = (unsigned char)( 120 + y );
			t[2] = 30;
			t[3] = d < 5 ? 255 : d == 5 || d == 11 ? 170 : 0;
		}
	}
	return p;
}

// 2x2 box filter, as the mipmaps are made
Pixels HalfSize( const Pixels &p, int size ) {
	const int half = size / 2;
	Pixels out( half * half * 4 );
	for ( int y = 0; y < half; y++ ) {
		for ( int x = 0; x < half; x++ ) {
			for ( int c = 0; c < 4; c++ ) {
				const int sum = p[( ( 2 * y ) * size + 2 * x ) * 4 + c] + p[( ( 2 * y ) * size + 2 * x + 1 ) * 4 + c] +
					p[( ( 2 * y + 1 ) * size + 2 * x ) * 4 + c] + p[( ( 2 * y + 1 ) * size + 2 * x + 1 ) * 4 + c];
				out[( y * half + x ) * 4 + c] = (unsigned char)( ( sum + 2 ) / 4 );
			}
		}
	}
	return out;
}

} // namespace

TEST(CoverageAlpha, KeepsTheAlphaTestResult) {
	// "alphaFunc GE128": a texel passes when its alpha is 128 or more
	for ( int a = 0; a < 256; a++ ) {
		EXPECT_EQ( R_CoverageAlpha( (unsigned char)a ) >= 128, a >= 128 ) << "alpha " << a;
	}
}

TEST(CoverageAlpha, IsMonotonic) {
	for ( int a = 0; a < 255; a++ ) {
		EXPECT_LE( R_CoverageAlpha( (unsigned char)a ), R_CoverageAlpha( (unsigned char)( a + 1 ) ) ) << "alpha " << a;
	}
}

TEST(CoverageAlpha, KeepsOpaqueAndTransparent) {
	EXPECT_EQ( R_CoverageAlpha( 0 ), 0 );
	EXPECT_EQ( R_CoverageAlpha( 255 ), 255 );
}

TEST(CoverageAlpha, OnlyTheThresholdKeepsPartialCoverage) {
	const int band = 128 / COVERAGE_SHARPNESS;	// texels on each side still partial

	for ( int a = 0; a < 256; a++ ) {
		const int c = R_CoverageAlpha( (unsigned char)a );
		if ( a >= 128 + band ) {
			EXPECT_EQ( c, 255 ) << "alpha " << a;
		} else if ( a < 128 - band ) {
			EXPECT_EQ( c, 0 ) << "alpha " << a;
		} else {
			EXPECT_GT( c, 0 ) << "alpha " << a;
			EXPECT_LT( c, 255 ) << "alpha " << a;
		}
	}
}

TEST(SharpenCoverageAlpha, ChangesOnlyTheAlpha) {
	const Pixels original = Blades( 32 );
	Pixels sharpened = original;

	EXPECT_NE( R_SharpenCoverageAlpha( sharpened.data(), 32 * 32 ), 0 );
	for ( size_t i = 0; i < original.size(); i += 4 ) {
		EXPECT_EQ( sharpened[i + 0], original[i + 0] );
		EXPECT_EQ( sharpened[i + 1], original[i + 1] );
		EXPECT_EQ( sharpened[i + 2], original[i + 2] );
		EXPECT_EQ( sharpened[i + 3], R_CoverageAlpha( original[i + 3] ) );
	}
}

TEST(SharpenCoverageAlpha, ReportsNothingToSharpen) {
	Pixels hard( 16 * 4 );
	for ( int i = 0; i < 16; i++ ) {
		hard[i * 4 + 3] = ( i & 1 ) ? 255 : 0;
	}
	EXPECT_EQ( R_SharpenCoverageAlpha( hard.data(), 16 ), 0 );
	EXPECT_EQ( R_SharpenCoverageAlpha( hard.data(), 0 ), 0 );
}

// Every mip level, sharpened after it was made from the plain level above:
// the alpha test result is that of the plain mipmaps, and where the plain
// alpha left the drawn texels see-through under alpha to coverage, and the
// discarded ones half drawn, the sharpened alpha covers them fully or not at
// all (down to the levels where the blades are a texel wide)
TEST(SharpenCoverageAlpha, MipmapsKeepTheCutOutAndFillIt) {
	int size = 64;
	Pixels level = Blades( size );

	for ( int mip = 0; size >= 4; mip++ ) {
		Pixels sharpened = level;
		R_SharpenCoverageAlpha( sharpened.data(), size * size );

		double plainIn = 0.0, sharpIn = 0.0, plainOut = 0.0, sharpOut = 0.0;
		int in = 0, out = 0;
		for ( int i = 0; i < size * size; i++ ) {
			const int a = level[i * 4 + 3], s = sharpened[i * 4 + 3];
			ASSERT_EQ( s >= 128, a >= 128 ) << "mip " << mip << " texel " << i;
			if ( a >= 128 ) {
				plainIn += a / 255.0;
				sharpIn += s / 255.0;
				in++;
			} else {
				plainOut += a / 255.0;
				sharpOut += s / 255.0;
				out++;
			}
		}
		if ( mip <= 2 ) {
			// mean coverage of the texels the alpha test draws, and of the others
			ASSERT_GT( in, 0 ) << "mip " << mip;
			ASSERT_GT( out, 0 ) << "mip " << mip;
			EXPECT_GE( sharpIn / in, 0.95 ) << "mip " << mip;
			EXPECT_LE( sharpOut / out, 0.05 ) << "mip " << mip;
			if ( mip >= 1 ) {
				EXPECT_LT( plainIn / in, 0.9 ) << "mip " << mip;
				EXPECT_GT( plainOut / out, 0.15 ) << "mip " << mip;
			}
		}

		level = HalfSize( level, size );
		size /= 2;
	}
}
