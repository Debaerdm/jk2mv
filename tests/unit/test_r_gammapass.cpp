// test_r_gammapass.cpp - what the post-process gamma pass shows, and the
// color grade of r_fbo built through it
//
// Links the real engine source (src/renderer/tr_gammapass.cpp), which has
// no engine dependency.

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include "renderer/tr_gammapass.h"

namespace {

struct GammaTable {
	unsigned char e[GAMMA_LUT_SIZE];
};

// the gamma table of R_SetColorMappings (post-process gamma)
GammaTable ClassicTable( float g, int shift ) {
	GammaTable t;
	for ( int i = 0; i < GAMMA_LUT_SIZE; i++ ) {
		int inf;
		if ( g == 1.0f ) {
			inf = (int)( ( (float)i / 63.0f ) * 255.0f + 0.5f );
		} else {
			inf = (int)( 255.0f * powf( i / 63.0f, 1.0f / g ) + 0.5f );
		}
		inf <<= shift;
		t.e[i] = (unsigned char)( inf > 255 ? 255 : inf );
	}
	return t;
}

// a grade on the gray axis: the contrast of the noir grade
float Contrast( float c ) {
	c = ( c - 0.5f ) * 1.15f + 0.5f;
	return c < 0.0f ? 0.0f : c > 1.0f ? 1.0f : c;
}

struct GradeError {
	double mean;
	int offBy2;		// framebuffer values shown more than 2 levels off
};

// r_fbo: the grade table as R_UploadGradeLUT builds it, applied to every 8
// bit framebuffer value as the grade pass samples it (texel centers, 16 bit
// entries), stored in the 8 bit scene, then shown by the gamma pass; against
// the grade of what the gamma pass shows without it
GradeError GradeThroughGammaPass( const GammaTable &t ) {
	double entries[GAMMA_LUT_SIZE];
	for ( int k = 0; k < GAMMA_LUT_SIZE; k++ ) {
		const float shown = R_GammaPassValue( t.e, k / 63.0f ) / 255.0f;
		const float v = R_GammaPassInverse( t.e, Contrast( shown ) * 255.0f );
		entries[k] = (int)( v * 65535.0f + 0.5f ) / 65535.0;
	}

	GradeError err = { 0.0, 0 };
	for ( int f8 = 0; f8 < 256; f8++ ) {
		const double p = f8 / 255.0 * 63.0;
		const int i = p >= 63.0 ? 62 : (int)p;
		const double graded = entries[i] + ( entries[i + 1] - entries[i] ) * ( p - i );
		const float stored = (int)( graded * 255.0 + 0.5 ) / 255.0f;
		const double shown = R_GammaPassValue( t.e, stored );
		const double intended = Contrast( R_GammaPassValue( t.e, f8 / 255.0f ) / 255.0f ) * 255.0;
		const double e = fabs( shown - intended );
		err.mean += e / 256.0;
		err.offBy2 += e > 2.0;
	}
	return err;
}

} // namespace

TEST(GammaPassValue, IsTheTableAtTexelCenters) {
	const GammaTable t = ClassicTable( 2.0f, 1 );
	for ( int i = 0; i < GAMMA_LUT_SIZE; i++ ) {
		EXPECT_FLOAT_EQ( R_GammaPassValue( t.e, ( i + 0.5f ) / GAMMA_LUT_SIZE ), t.e[i] ) << "entry " << i;
	}
}

TEST(GammaPassValue, InterpolatesAndClamps) {
	const GammaTable t = ClassicTable( 1.0f, 0 );
	EXPECT_FLOAT_EQ( R_GammaPassValue( t.e, 0.0f ), t.e[0] );
	EXPECT_FLOAT_EQ( R_GammaPassValue( t.e, -1.0f ), t.e[0] );
	EXPECT_FLOAT_EQ( R_GammaPassValue( t.e, 1.0f ), t.e[GAMMA_LUT_SIZE - 1] );
	EXPECT_FLOAT_EQ( R_GammaPassValue( t.e, 2.0f ), t.e[GAMMA_LUT_SIZE - 1] );
	// half way between the centers of entries 10 and 11
	EXPECT_FLOAT_EQ( R_GammaPassValue( t.e, 11.0f / GAMMA_LUT_SIZE ), ( t.e[10] + t.e[11] ) * 0.5f );
}

TEST(GammaPassInverse, ShowsWhatWasAskedFor) {
	const float gammas[] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f };
	for ( float g : gammas ) {
		for ( int shift = 0; shift <= 1; shift++ ) {
			const GammaTable t = ClassicTable( g, shift );
			for ( float d = t.e[0]; d <= t.e[GAMMA_LUT_SIZE - 1]; d += 0.25f ) {
				EXPECT_NEAR( R_GammaPassValue( t.e, R_GammaPassInverse( t.e, d ) ), d, 1e-3 )
					<< "r_gamma " << g << " overbright " << shift << " shown " << d;
			}
		}
	}
}

TEST(GammaPassInverse, IsTheLowestFramebufferValue) {
	// with overbright the upper half of the framebuffer values is white
	const GammaTable t = ClassicTable( 1.0f, 1 );
	int firstWhite = 0;
	while ( t.e[firstWhite] < 255 ) {
		firstWhite++;
	}
	EXPECT_FLOAT_EQ( R_GammaPassInverse( t.e, 255.0f ), ( firstWhite + 0.5f ) / GAMMA_LUT_SIZE );
	EXPECT_FLOAT_EQ( R_GammaPassInverse( t.e, 300.0f ), ( firstWhite + 0.5f ) / GAMMA_LUT_SIZE );
	EXPECT_FLOAT_EQ( R_GammaPassInverse( t.e, 0.0f ), 0.0f );
	EXPECT_FLOAT_EQ( R_GammaPassInverse( t.e, -5.0f ), 0.0f );

	const GammaTable dark = ClassicTable( 0.5f, 0 );	// starts with equal entries
	for ( float d = 0.5f; d < 255.0f; d += 0.5f ) {
		const float v = R_GammaPassInverse( dark.e, d );
		EXPECT_LT( R_GammaPassValue( dark.e, v - 1e-4f ), d ) << "shown " << d;
	}
}

// the color grade of r_fbo shows what the grade of the classic image would,
// within the steps of the 8 bit scene, also at r_gamma above 1 where the
// gamma pass is steep near black
TEST(GammaPassGrade, MatchesTheGradeOfTheShownImage) {
	const float gammas[] = { 1.0f, 1.5f, 2.0f, 3.0f };
	for ( float g : gammas ) {
		for ( int shift = 0; shift <= 1; shift++ ) {
			const GradeError err = GradeThroughGammaPass( ClassicTable( g, shift ) );
			EXPECT_LT( err.mean, 0.6 ) << "r_gamma " << g << " overbright " << shift;
			EXPECT_LE( err.offBy2, 8 ) << "r_gamma " << g << " overbright " << shift;
		}
	}
}
