// tr_coverage.h -- alpha of the cut-out textures drawn with alpha to coverage
//
// No engine dependency, so the unit tests link the real source.

#ifndef TR_COVERAGE_H
#define TR_COVERAGE_H

// how much the alpha is stretched away from the alpha test threshold
#define COVERAGE_SHARPNESS	8

// The alpha of a texel for alpha to coverage (r_ext_alphaToCoverage), which
// replaces the "alphaFunc GE128" test: stretched COVERAGE_SHARPNESS times
// away from the threshold, so a texel passes the test (128 and up) exactly
// when it passed before, but the inside of a leaf or a bar reaches full
// coverage and the outside none. Only texels close to the threshold, the
// edges, keep a partial coverage, which antialiases them.
unsigned char	R_CoverageAlpha( unsigned char alpha );

// R_CoverageAlpha on the alpha of count RGBA (or BGRA) pixels, in place.
// Returns nonzero if any alpha changed.
int				R_SharpenCoverageAlpha( unsigned char *pixels, int count );

#endif // TR_COVERAGE_H
