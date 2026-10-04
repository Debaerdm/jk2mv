// cl_fov.cpp -- widescreen field of view math of cl_fovAspectFix (see cl_fov.h)

#include "cl_fov.h"
#include <math.h>

bool CL_FovWiden( float fovX, float fovY, int width, int height, float *outX, float *outY ) {
	if ( width <= 0 || height <= 0 || width * 3 <= height * 4 ||
		!( fovX > 0.0f && fovX < 180.0f ) || !( fovY > 0.0f && fovY < 180.0f ) ) {
		return false;
	}
	const double halfRad = 3.14159265358979323846 / 360.0;	// degrees to radians, halved
	const double k = 3.0 * width / ( 4.0 * height );

	*outX = (float)( atan( k * tan( fovX * halfRad ) ) / halfRad );
	*outY = (float)( atan( k * tan( fovY * halfRad ) ) / halfRad );
	return true;
}
