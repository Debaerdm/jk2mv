// cl_fov.h -- widescreen field of view math of cl_fovAspectFix (cl_cgame.cpp)
//
// No engine dependency, so the unit tests can link it. Angles in degrees.

#ifndef CL_FOV_H
#define CL_FOV_H

// Widens the fov of a view drawn for a 4:3 screen to a width x height screen
// wider than 4:3, keeping the 4:3 vertical view ("Hor+"): the tangents of
// both half angles grow by 3 * width / (4 * height). On a fov_y that the
// cgame derived from fov_x, that is the 4:3 fov_y; and whatever the cgame did
// to fov_y on its own (the underwater warp moves fov_x and fov_y apart) is
// kept. Returns false, leaving the outputs alone, on screens up to 4:3 and
// for fovs outside (0, 180).
bool CL_FovWiden( float fovX, float fovY, int width, int height, float *outX, float *outY );

#endif // CL_FOV_H
