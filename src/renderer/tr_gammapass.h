// tr_gammapass.h -- what the post-process gamma pass shows
//
// No engine dependency, so the unit tests link the real source.

#ifndef TR_GAMMAPASS_H
#define TR_GAMMAPASS_H

#define GAMMA_LUT_SIZE	64		// entries per channel of the gamma table

// The value shown (0 to 255) for the framebuffer value v (0 to 1) by the
// gamma pass, which samples the table of R_SetColorMappings with linear
// filtering at texel 64v - 0.5, clamped to its first and last entries.
float	R_GammaPassValue( const unsigned char *table, float v );

// The lowest framebuffer value (0 to 1) the gamma pass shows as d (0 to 255):
// the inverse of R_GammaPassValue, 0 at or below the first entry, the first
// value showing the last entry at or above it.
float	R_GammaPassInverse( const unsigned char *table, float d );

#endif // TR_GAMMAPASS_H
