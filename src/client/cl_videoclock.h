// cl_videoclock.h -- the fixed time steps of video recording
//
// No engine dependency, so it can be unit tested.

#ifndef CL_VIDEOCLOCK_H
#define CL_VIDEOCLOCK_H

#include <math.h>

// The engine counts whole milliseconds: more than 1000 engine frames per
// second would give frames without time of their own.
#define VIDEO_MAX_ENGINE_FPS	1000

/*
Engine frames per video frame for cl_aviMotionBlur: 2 to 32, and no more
than VIDEO_MAX_ENGINE_FPS engine frames per second in all. 1 is no blur.
*/
static inline int CL_VideoBlendFrames( int requested, int frameRate ) {
	int		frames, most;

	if ( requested < 2 ) {
		return 1;
	}
	frames = requested > 32 ? 32 : requested;
	most = frameRate > 0 ? VIDEO_MAX_ENGINE_FPS / frameRate : 1;
	if ( frames > most ) {
		frames = most > 1 ? most : 1;
	}
	return frames;
}

/*
Game time of an engine frame while recording: frameTime ms (the engine
frame's share of the video frame, times the timescale) in whole
milliseconds, the fraction carried over in *overflow. Never negative: in
slow motion a frame can get less than a millisecond, and then 0.
*/
static inline int CL_VideoFrameMsec( double frameTime, double *overflow ) {
	const double	time = frameTime + *overflow;
	const int		msec = (int)floor( time );

	*overflow = time - msec;
	return msec;
}

#endif // CL_VIDEOCLOCK_H
