// cl_campath.h -- camera path interpolation for demo cinematics
//
// Pure math with no engine dependency, so the unit tests can link it.
// Angles follow the engine convention: [0] pitch (positive looks down),
// [1] yaw, [2] roll, in degrees.

#ifndef CL_CAMPATH_H
#define CL_CAMPATH_H

typedef struct {
	int		time;			// demo server time in milliseconds
	float	origin[3];
	float	angles[3];
	float	fov;			// horizontal field of view, 0 keeps the game's
	float	timescale;		// playback speed at this key
} camKey_t;

typedef struct {
	float	origin[3];
	float	angles[3];
	float	fov;
	float	timescale;
} camView_t;

// Evaluates a path of keys sorted by time. Positions follow a Hermite spline
// with Catmull-Rom tangents scaled to the uneven key spacing, orientations a
// quaternion slerp, fov and timescale a linear blend. Before the first key
// and after the last one the end keys hold. Returns false without keys.
bool CamPath_Evaluate( const camKey_t *keys, int numKeys, float time, camView_t *out );

// Inserts a key keeping the array sorted, replacing a key at the same time.
// Returns the new key count, or -1 when the array is full.
int CamPath_Insert( camKey_t *keys, int numKeys, int maxKeys, const camKey_t *key );

void CamPath_AnglesToQuat( const float angles[3], float q[4] );
void CamPath_QuatToAngles( const float q[4], float angles[3] );
void CamPath_Slerp( const float from[4], const float to[4], float t, float out[4] );

#endif // CL_CAMPATH_H
