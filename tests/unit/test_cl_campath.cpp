// test_cl_campath.cpp - camera path math of the demo tools
//
// Unlike the server tests, this links the real engine source
// (src/client/cl_campath.cpp), which has no engine dependency.

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include "client/cl_campath.h"

namespace {

// the engine's AngleVectors convention, used as the reference
void Forward( const float angles[3], double f[3] ) {
	const double d = 3.14159265358979323846 / 180.0;
	const double sp = sin( angles[0] * d ), cp = cos( angles[0] * d );
	const double sy = sin( angles[1] * d ), cy = cos( angles[1] * d );
	f[0] = cp * cy;
	f[1] = cp * sy;
	f[2] = -sp;
}

void Up( const float angles[3], double u[3] ) {
	const double d = 3.14159265358979323846 / 180.0;
	const double sp = sin( angles[0] * d ), cp = cos( angles[0] * d );
	const double sy = sin( angles[1] * d ), cy = cos( angles[1] * d );
	const double sr = sin( angles[2] * d ), cr = cos( angles[2] * d );
	u[0] = cr * sp * cy + sr * sy;
	u[1] = cr * sp * sy - sr * cy;
	u[2] = cr * cp;
}

void ExpectSameOrientation( const float a[3], const float b[3], double tolerance = 1e-4 ) {
	double fa[3], fb[3], ua[3], ub[3];
	Forward( a, fa );
	Forward( b, fb );
	Up( a, ua );
	Up( b, ub );
	for ( int i = 0; i < 3; i++ ) {
		EXPECT_NEAR( fa[i], fb[i], tolerance );
		EXPECT_NEAR( ua[i], ub[i], tolerance );
	}
}

camKey_t Key( int time, float x, float y, float z, float pitch, float yaw, float roll, float fov = 90.0f, float timescale = 1.0f ) {
	camKey_t k = { time, { x, y, z }, { pitch, yaw, roll }, fov, timescale };
	return k;
}

} // namespace

TEST(CamPathQuat, KnownYawRotation) {
	const float angles[3] = { 0.0f, 90.0f, 0.0f };
	float q[4];
	CamPath_AnglesToQuat( angles, q );
	EXPECT_NEAR( q[0], 0.0, 1e-6 );
	EXPECT_NEAR( q[1], 0.0, 1e-6 );
	EXPECT_NEAR( q[2], std::sqrt( 0.5 ), 1e-6 );
	EXPECT_NEAR( q[3], std::sqrt( 0.5 ), 1e-6 );
}

TEST(CamPathQuat, RoundTripKeepsOrientation) {
	srand( 1234 );
	for ( int n = 0; n < 2000; n++ ) {
		const float angles[3] = {
			-85.0f + 170.0f * rand() / (float)RAND_MAX,
			-180.0f + 360.0f * rand() / (float)RAND_MAX,
			-180.0f + 360.0f * rand() / (float)RAND_MAX };
		float q[4], back[3];
		CamPath_AnglesToQuat( angles, q );
		EXPECT_NEAR( q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3], 1.0, 1e-5 );
		CamPath_QuatToAngles( q, back );
		ExpectSameOrientation( angles, back );
	}
}

TEST(CamPathQuat, LookingStraightDown) {
	const float angles[3] = { 90.0f, 30.0f, 0.0f };
	float q[4], back[3];
	CamPath_AnglesToQuat( angles, q );
	CamPath_QuatToAngles( q, back );
	ExpectSameOrientation( angles, back );
}

TEST(CamPathSlerp, EndpointsAndMidpoint) {
	const float a[3] = { 0.0f, 0.0f, 0.0f }, b[3] = { 0.0f, 90.0f, 0.0f };
	float qa[4], qb[4], q[4], angles[3];
	CamPath_AnglesToQuat( a, qa );
	CamPath_AnglesToQuat( b, qb );

	CamPath_Slerp( qa, qb, 0.0f, q );
	CamPath_QuatToAngles( q, angles );
	ExpectSameOrientation( angles, a );

	CamPath_Slerp( qa, qb, 1.0f, q );
	CamPath_QuatToAngles( q, angles );
	ExpectSameOrientation( angles, b );

	CamPath_Slerp( qa, qb, 0.5f, q );
	CamPath_QuatToAngles( q, angles );
	const float mid[3] = { 0.0f, 45.0f, 0.0f };
	ExpectSameOrientation( angles, mid );
}

TEST(CamPathSlerp, TakesTheShortWay) {
	// 170 to -170 degrees of yaw crosses 180, not 0
	const float a[3] = { 0.0f, 170.0f, 0.0f }, b[3] = { 0.0f, -170.0f, 0.0f };
	float qa[4], qb[4], q[4], angles[3];
	CamPath_AnglesToQuat( a, qa );
	CamPath_AnglesToQuat( b, qb );
	CamPath_Slerp( qa, qb, 0.5f, q );
	CamPath_QuatToAngles( q, angles );
	const float mid[3] = { 0.0f, 180.0f, 0.0f };
	ExpectSameOrientation( angles, mid );
}

TEST(CamPathEvaluate, NoKeys) {
	camView_t v;
	EXPECT_FALSE( CamPath_Evaluate( nullptr, 0, 0.0f, &v ) );
}

TEST(CamPathEvaluate, HoldsOutsideThePath) {
	const camKey_t keys[2] = { Key( 1000, 0, 0, 0, 0, 0, 0, 80, 1 ), Key( 2000, 100, 0, 0, 0, 90, 0, 100, 0.5f ) };
	camView_t v;

	ASSERT_TRUE( CamPath_Evaluate( keys, 2, 500.0f, &v ) );
	EXPECT_FLOAT_EQ( v.origin[0], 0.0f );
	EXPECT_FLOAT_EQ( v.fov, 80.0f );

	ASSERT_TRUE( CamPath_Evaluate( keys, 2, 2500.0f, &v ) );
	EXPECT_FLOAT_EQ( v.origin[0], 100.0f );
	EXPECT_FLOAT_EQ( v.fov, 100.0f );
	EXPECT_FLOAT_EQ( v.timescale, 0.5f );
}

TEST(CamPathEvaluate, PassesThroughEveryKey) {
	const camKey_t keys[4] = {
		Key( 0, 0, 0, 0, 0, 0, 0 ), Key( 700, 100, 50, 20, 10, 45, 0 ),
		Key( 1000, 150, -30, 40, -20, 120, 15 ), Key( 2500, 0, -100, 0, 0, 200, 0 ) };
	for ( int i = 0; i < 4; i++ ) {
		camView_t v;
		ASSERT_TRUE( CamPath_Evaluate( keys, 4, (float)keys[i].time, &v ) );
		for ( int axis = 0; axis < 3; axis++ ) {
			EXPECT_NEAR( v.origin[axis], keys[i].origin[axis], 1e-3 );
		}
		ExpectSameOrientation( v.angles, keys[i].angles );
	}
}

TEST(CamPathEvaluate, TwoKeysMoveInAStraightLineAtConstantSpeed) {
	const camKey_t keys[2] = { Key( 0, 0, 0, 0, 0, 0, 0 ), Key( 1000, 100, 200, -50, 0, 0, 0 ) };
	for ( int t = 0; t <= 1000; t += 125 ) {
		camView_t v;
		ASSERT_TRUE( CamPath_Evaluate( keys, 2, (float)t, &v ) );
		EXPECT_NEAR( v.origin[0], 0.1 * t, 1e-3 );
		EXPECT_NEAR( v.origin[1], 0.2 * t, 1e-3 );
		EXPECT_NEAR( v.origin[2], -0.05 * t, 1e-3 );
	}
}

TEST(CamPathEvaluate, VelocityIsContinuousAcrossKeys) {
	// unevenly spaced keys: the tangents must still match on both sides
	const camKey_t keys[3] = { Key( 0, 0, 0, 0, 0, 0, 0 ), Key( 300, 100, 80, 0, 0, 0, 0 ), Key( 2000, 400, -50, 30, 0, 0, 0 ) };
	const float h = 0.01f;
	camView_t before, at, after;
	ASSERT_TRUE( CamPath_Evaluate( keys, 3, 300.0f - h, &before ) );
	ASSERT_TRUE( CamPath_Evaluate( keys, 3, 300.0f, &at ) );
	ASSERT_TRUE( CamPath_Evaluate( keys, 3, 300.0f + h, &after ) );
	for ( int axis = 0; axis < 3; axis++ ) {
		const double left = ( at.origin[axis] - before.origin[axis] ) / h;
		const double right = ( after.origin[axis] - at.origin[axis] ) / h;
		EXPECT_NEAR( left, right, 0.02 );
	}
}

TEST(CamPathEvaluate, FovAndTimescaleBlendLinearly) {
	const camKey_t keys[2] = { Key( 0, 0, 0, 0, 0, 0, 0, 60, 1.0f ), Key( 1000, 0, 0, 0, 0, 0, 0, 100, 0.25f ) };
	camView_t v;
	ASSERT_TRUE( CamPath_Evaluate( keys, 2, 250.0f, &v ) );
	EXPECT_NEAR( v.fov, 70.0, 1e-4 );
	EXPECT_NEAR( v.timescale, 0.8125, 1e-5 );
}

TEST(CamPathInsert, KeepsKeysSortedAndReplacesSameTime) {
	camKey_t keys[3];
	int n = 0;
	const camKey_t k2 = Key( 2000, 2, 0, 0, 0, 0, 0 ), k1 = Key( 1000, 1, 0, 0, 0, 0, 0 ), k1b = Key( 1000, 9, 0, 0, 0, 0, 0 );
	n = CamPath_Insert( keys, n, 3, &k2 );
	n = CamPath_Insert( keys, n, 3, &k1 );
	ASSERT_EQ( n, 2 );
	EXPECT_EQ( keys[0].time, 1000 );
	EXPECT_EQ( keys[1].time, 2000 );

	n = CamPath_Insert( keys, n, 3, &k1b );
	ASSERT_EQ( n, 2 );
	EXPECT_FLOAT_EQ( keys[0].origin[0], 9.0f );

	const camKey_t k3 = Key( 3000, 3, 0, 0, 0, 0, 0 ), k4 = Key( 4000, 4, 0, 0, 0, 0, 0 );
	n = CamPath_Insert( keys, n, 3, &k3 );
	ASSERT_EQ( n, 3 );
	EXPECT_EQ( CamPath_Insert( keys, n, 3, &k4 ), -1 );
}

TEST(CamPathEvaluate, LargeServerTimesKeepMillisecondPrecision) {
	// public servers run for days: demo server times pass 2^24 ms
	const int base = 1000000000;
	const camKey_t keys[2] = { Key( base, 0, 0, 0, 0, 0, 0 ), Key( base + 1000, 1000, 0, 0, 0, 90, 0 ) };
	camView_t v;
	float last = -1.0f;
	int distinct = 0;
	for ( int t = 0; t < 1000; t++ ) {
		ASSERT_TRUE( CamPath_Evaluate( keys, 2, (double)base + t, &v ) );
		if ( v.origin[0] != last ) {
			distinct++;
			last = v.origin[0];
		}
	}
	EXPECT_GE( distinct, 999 );
}

static double YawRate( const camKey_t *keys, int n, double t, double h ) {
	camView_t a, b;
	CamPath_Evaluate( keys, n, t - h, &a );
	CamPath_Evaluate( keys, n, t + h, &b );
	return ( b.angles[1] - a.angles[1] ) / ( 2.0 * h );
}

TEST(CamPathEvaluate, TurnRateIsContinuousAtUnevenKeys) {
	// a pan that slows down: plain slerp jumped from 90 to 150 degrees/s
	const camKey_t keys[3] = { Key( 0, 0, 0, 0, 0, 0, 0 ), Key( 1000, 1000, 0, 0, 0, 90, 0 ), Key( 1200, 1200, 0, 0, 0, 120, 0 ) };
	const double before = YawRate( keys, 3, 999.0, 0.5 );
	const double after = YawRate( keys, 3, 1001.0, 0.5 );
	EXPECT_NEAR( after, before, 0.02 * fabs( before ) ) << before << " " << after;

	// still goes through the keys
	camView_t v;
	ASSERT_TRUE( CamPath_Evaluate( keys, 3, 1000.0, &v ) );
	EXPECT_NEAR( v.angles[1], 90.0, 1e-3 );
}

TEST(CamPathEvaluate, EvenKeysTurnAtTheCatmullRomRate) {
	// evenly spaced 0, 90, 180 degree keys: 0.09 degrees/ms through the middle
	const camKey_t keys[3] = { Key( 0, 0, 0, 0, 0, 0, 0 ), Key( 1000, 0, 0, 0, 0, 90, 0 ), Key( 2000, 0, 0, 0, 0, 180, 0 ) };
	EXPECT_NEAR( YawRate( keys, 3, 1000.0, 0.5 ), 0.09, 0.002 );
}
