// tr_coverage.cpp -- alpha of the cut-out textures drawn with alpha to
// coverage (see R_CreateCoverageImage in tr_image.cpp)

#include "tr_coverage.h"

unsigned char R_CoverageAlpha( unsigned char alpha ) {
	// 127 and 128 stay on their side of the threshold (127.5 of 255)
	const int stretched = alpha >= 128 ?
		128 + ( alpha - 128 ) * COVERAGE_SHARPNESS :
		127 - ( 127 - alpha ) * COVERAGE_SHARPNESS;

	if ( stretched < 0 ) {
		return 0;
	}
	if ( stretched > 255 ) {
		return 255;
	}
	return (unsigned char)stretched;
}

int R_SharpenCoverageAlpha( unsigned char *pixels, int count ) {
	int changed = 0;

	for ( int i = 0; i < count; i++, pixels += 4 ) {
		const unsigned char alpha = R_CoverageAlpha( pixels[3] );

		if ( alpha != pixels[3] ) {
			pixels[3] = alpha;
			changed = 1;
		}
	}
	return changed;
}
