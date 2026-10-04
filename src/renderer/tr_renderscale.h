// tr_renderscale.h -- render size and resampling of r_renderScale
//
// No engine dependency, so the unit tests link the real source.

#ifndef TR_RENDERSCALE_H
#define TR_RENDERSCALE_H

#define RENDERSCALE_MIN		0.5f
#define RENDERSCALE_MAX		2.0f

// the scale asked for by r_renderScale: 0.5 to 2, and 1 (off) for 0 or less
float	R_RenderScaleValue( float cvarValue );

// size the frames are rendered at for a window drawable of windowWidth x
// windowHeight. Both sides keep the same scale, lowered when needed so that
// neither goes past maxSize, the largest target the GL can make (0: no limit).
void	R_RenderScaleSize( int windowWidth, int windowHeight, float scale, int maxSize, int *width, int *height );

// a length of side texels of a frame of frame texels shown on shown pixels,
// in shown pixels: at least 1, and side itself when frame == shown
int		R_RenderScaleShownSide( int side, int frame, int shown );

// offset, in source texels, of four bilinear taps at (+-offset, +-offset)
// shrinking an image by ratio source texels per destination texel (1 to 2):
// together they cover the destination texel's footprint and nothing else,
// an exact 2x2 average at 2, and weigh the source texels almost evenly at
// the ratios in between, where a single bilinear tap skips some of them
float	R_RenderScaleBoxOffset( float ratio );

#endif // TR_RENDERSCALE_H
