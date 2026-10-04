// tr_renderscale.cpp -- render size and resampling of r_renderScale (see
// tr_postfx.cpp)

#include "tr_renderscale.h"

float R_RenderScaleValue( float cvarValue ) {
	if ( !( cvarValue > 0.0f ) ) {		// also NaN
		return 1.0f;
	}
	if ( cvarValue < RENDERSCALE_MIN ) {
		return RENDERSCALE_MIN;
	}
	if ( cvarValue > RENDERSCALE_MAX ) {
		return RENDERSCALE_MAX;
	}
	return cvarValue;
}

static int R_ScaleSide( int side, double scale, int maxSize ) {
	int scaled = (int)( side * scale + 0.5 );

	if ( scaled < 1 ) {
		scaled = 1;
	}
	if ( maxSize > 0 && scaled > maxSize ) {
		scaled = maxSize;
	}
	return scaled;
}

void R_RenderScaleSize( int windowWidth, int windowHeight, float scale, int maxSize, int *width, int *height ) {
	double s = scale;

	// nothing to scale (a window being created or minimized)
	if ( windowWidth <= 0 || windowHeight <= 0 ) {
		*width = windowWidth;
		*height = windowHeight;
		return;
	}
	if ( maxSize > 0 ) {
		const int longest = windowWidth > windowHeight ? windowWidth : windowHeight;

		if ( longest * s > maxSize ) {
			s = (double)maxSize / longest;
		}
	}
	*width = R_ScaleSide( windowWidth, s, maxSize );
	*height = R_ScaleSide( windowHeight, s, maxSize );
}

int R_RenderScaleShownSide( int side, int frame, int shown ) {
	if ( frame <= 0 || shown <= 0 || frame == shown ) {
		return side;
	}
	return R_ScaleSide( side, (double)shown / frame, 0 );
}

float R_RenderScaleBoxOffset( float ratio ) {
	// a quarter of the footprint from its center, so the taps are half of it
	// apart: at 2, on the centers of the 2x2 texels
	return ratio * 0.25f;
}
