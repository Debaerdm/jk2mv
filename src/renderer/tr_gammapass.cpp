// tr_gammapass.cpp -- what the post-process gamma pass shows (see
// R_UploadGradeLUT in tr_image.cpp)

#include "tr_gammapass.h"

float R_GammaPassValue( const unsigned char *table, float v ) {
	const float p = v * GAMMA_LUT_SIZE - 0.5f;

	if ( !( p > 0.0f ) ) {
		return table[0];
	}
	if ( p >= GAMMA_LUT_SIZE - 1 ) {
		return table[GAMMA_LUT_SIZE - 1];
	}
	const int i = (int)p;
	return table[i] + ( table[i + 1] - table[i] ) * ( p - i );
}

float R_GammaPassInverse( const unsigned char *table, float d ) {
	if ( !( d > table[0] ) ) {
		return 0.0f;
	}
	if ( d > table[GAMMA_LUT_SIZE - 1] ) {
		d = table[GAMMA_LUT_SIZE - 1];
	}
	// d is above entry i: the first entry reaching it ends its segment
	for ( int i = 0; i < GAMMA_LUT_SIZE - 1; i++ ) {
		if ( d <= table[i + 1] ) {
			const float t = ( d - table[i] ) / ( table[i + 1] - table[i] );
			return ( i + t + 0.5f ) / GAMMA_LUT_SIZE;
		}
	}
	return 1.0f;	// not reached: d was clamped to the last entry
}
