// test_cl_fov.cpp - widescreen field of view math of cl_fovAspectFix
//
// Like test_cl_campath, this links the real engine source
// (src/client/cl_fov.cpp), which has no engine dependency.

#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include "client/cl_fov.h"

namespace {

const double kPi = 3.14159265358979323846;

double TanHalf( double fovDeg ) {
	return tan( fovDeg * kPi / 360.0 );
}

// the fov_y the base cgame derives from fov_x (CG_CalcFov): the same
// horizontal fov on any screen ("Vert-")
double CgameFovY( double fovX, int width, int height ) {
	const double x = width / TanHalf( fovX );
	return 2.0 * atan2( (double)height, x ) * 180.0 / kPi;
}

// MVSDK's cg_fovAspectAdjust (cg_view.c CG_CalcFov), the reference Hor+,
// also the formula of the previous CL_FovAspectFix
void MvsdkHorPlus( double fovX, int width, int height, double *outX, double *outY ) {
	const double x = height * ( 4.0 / 3.0 ) / TanHalf( fovX );
	*outX = 2.0 * atan2( (double)width, x ) * 180.0 / kPi;
	*outY = 2.0 * atan2( (double)height, x ) * 180.0 / kPi;
}

struct Screen {
	int width, height;
};

// 16:10, 16:9, 21:9, 32:9 and just wider than 4:3
const Screen kWide[] = { { 1280, 800 }, { 1280, 720 }, { 1920, 1080 }, { 2560, 1080 }, { 3440, 1440 }, { 5120, 1440 }, { 1281, 960 } };

} // namespace

TEST(ClFov, EightyDegreesOnSixteenByNine) {
	float x, y;
	ASSERT_TRUE( CL_FovWiden( 80.0f, (float)CgameFovY( 80.0, 1280, 720 ), 1280, 720, &x, &y ) );
	EXPECT_NEAR( x, 96.4, 0.05 );
	EXPECT_NEAR( y, 64.4, 0.05 );
}

TEST(ClFov, MatchesMvsdkFovAspectAdjust) {
	// cgame fovs from the 1 degree disruptor zoom to the cg_fov cap of 160
	for ( const Screen &s : kWide ) {
		for ( double fov = 1.0; fov <= 160.0; fov += 0.5 ) {
			double mx, my;
			float x, y;
			MvsdkHorPlus( fov, s.width, s.height, &mx, &my );
			ASSERT_TRUE( CL_FovWiden( (float)fov, (float)CgameFovY( fov, s.width, s.height ), s.width, s.height, &x, &y ) );
			EXPECT_NEAR( x, mx, 2e-3 ) << s.width << "x" << s.height << " fov " << fov;
			EXPECT_NEAR( y, my, 2e-3 ) << s.width << "x" << s.height << " fov " << fov;
		}
	}
}

TEST(ClFov, KeepsTheFourByThreeVerticalView) {
	for ( const Screen &s : kWide ) {
		for ( double fov = 10.0; fov <= 150.0; fov += 10.0 ) {
			float x, y;
			ASSERT_TRUE( CL_FovWiden( (float)fov, (float)CgameFovY( fov, s.width, s.height ), s.width, s.height, &x, &y ) );
			EXPECT_NEAR( y, CgameFovY( fov, 640, 480 ), 2e-3 ) << s.width << "x" << s.height << " fov " << fov;
			EXPECT_GT( x, fov );
		}
	}
}

TEST(ClFov, KeepsTheUnderwaterWarp) {
	// In water, slime or lava, CG_CalcFov adds v = sin(phase) to fov_x and
	// takes it from fov_y: the view stretches one way and squashes the other.
	// Widened, it must still do that (the same tangent ratio as the cgame's
	// view), not zoom in and out as a whole (a ratio of width / height).
	const int w = 1280, h = 720;
	const double fov = 80.0, fovY = CgameFovY( fov, w, h );
	float x0, y0;
	ASSERT_TRUE( CL_FovWiden( (float)fov, (float)fovY, w, h, &x0, &y0 ) );
	for ( double v = -1.0; v <= 1.0; v += 0.25 ) {
		float x, y;
		ASSERT_TRUE( CL_FovWiden( (float)( fov + v ), (float)( fovY - v ), w, h, &x, &y ) );
		EXPECT_NEAR( TanHalf( x ) / TanHalf( y ), TanHalf( fov + v ) / TanHalf( fovY - v ), 1e-4 ) << "v " << v;
		if ( v > 0.0 ) {
			EXPECT_GT( x, x0 ) << "v " << v;
			EXPECT_LT( y, y0 ) << "v " << v;
		} else if ( v < 0.0 ) {
			EXPECT_LT( x, x0 ) << "v " << v;
			EXPECT_GT( y, y0 ) << "v " << v;
		}
	}
}

TEST(ClFov, NothingUpToFourByThree) {
	const Screen narrow[] = { { 640, 480 }, { 1280, 960 }, { 1280, 1024 }, { 720, 1280 }, { 0, 480 }, { 640, 0 } };
	for ( const Screen &s : narrow ) {
		float x = -1.0f, y = -1.0f;
		EXPECT_FALSE( CL_FovWiden( 80.0f, 60.0f, s.width, s.height, &x, &y ) ) << s.width << "x" << s.height;
		EXPECT_EQ( x, -1.0f );
		EXPECT_EQ( y, -1.0f );
	}
}

TEST(ClFov, RejectsFovsOutOfRange) {
	const float bad[] = { 0.0f, -10.0f, 180.0f, 200.0f,
		std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() };
	for ( float f : bad ) {
		float x = -1.0f, y = -1.0f;
		EXPECT_FALSE( CL_FovWiden( f, 60.0f, 1280, 720, &x, &y ) ) << f;
		EXPECT_FALSE( CL_FovWiden( 80.0f, f, 1280, 720, &x, &y ) ) << f;
		EXPECT_EQ( x, -1.0f );
		EXPECT_EQ( y, -1.0f );
	}
}
