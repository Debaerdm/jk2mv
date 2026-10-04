// cl_campath.cpp -- camera path interpolation for demo cinematics (see cl_campath.h)

#include "cl_campath.h"
#include <math.h>
#include <string.h>

static const double DEG2RAD = 3.14159265358979323846 / 180.0;

// rotation matrix whose columns are the forward, left and up vectors of the
// engine's AngleVectors convention
static void AnglesToMatrix( const float angles[3], double m[3][3] ) {
	const double sp = sin( angles[0] * DEG2RAD ), cp = cos( angles[0] * DEG2RAD );
	const double sy = sin( angles[1] * DEG2RAD ), cy = cos( angles[1] * DEG2RAD );
	const double sr = sin( angles[2] * DEG2RAD ), cr = cos( angles[2] * DEG2RAD );
	const double forward[3] = { cp * cy, cp * sy, -sp };
	const double right[3] = { -sr * sp * cy + cr * sy, -sr * sp * sy - cr * cy, -sr * cp };
	const double up[3] = { cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp };

	for ( int i = 0; i < 3; i++ ) {
		m[i][0] = forward[i];
		m[i][1] = -right[i];
		m[i][2] = up[i];
	}
}

static void MatrixToAngles( const double m[3][3], float angles[3] ) {
	const double fz = m[2][0] > 1.0 ? 1.0 : m[2][0] < -1.0 ? -1.0 : m[2][0];
	const double pitch = asin( -fz );

	if ( fabs( cos( pitch ) ) > 1e-6 ) {
		angles[1] = (float)( atan2( m[1][0], m[0][0] ) / DEG2RAD );
		angles[2] = (float)( atan2( m[2][1], m[2][2] ) / DEG2RAD );
	} else {
		// looking straight up or down: roll and yaw are the same axis
		angles[1] = (float)( atan2( -m[0][1], m[1][1] ) / DEG2RAD );
		angles[2] = 0.0f;
	}
	angles[0] = (float)( pitch / DEG2RAD );
}

void CamPath_AnglesToQuat( const float angles[3], float q[4] ) {
	double m[3][3], x, y, z, w;

	AnglesToMatrix( angles, m );
	const double t = m[0][0] + m[1][1] + m[2][2];

	if ( t > 0.0 ) {
		const double s = sqrt( t + 1.0 ) * 2.0;
		w = 0.25 * s;
		x = ( m[2][1] - m[1][2] ) / s;
		y = ( m[0][2] - m[2][0] ) / s;
		z = ( m[1][0] - m[0][1] ) / s;
	} else if ( m[0][0] > m[1][1] && m[0][0] > m[2][2] ) {
		const double s = sqrt( 1.0 + m[0][0] - m[1][1] - m[2][2] ) * 2.0;
		w = ( m[2][1] - m[1][2] ) / s;
		x = 0.25 * s;
		y = ( m[0][1] + m[1][0] ) / s;
		z = ( m[0][2] + m[2][0] ) / s;
	} else if ( m[1][1] > m[2][2] ) {
		const double s = sqrt( 1.0 + m[1][1] - m[0][0] - m[2][2] ) * 2.0;
		w = ( m[0][2] - m[2][0] ) / s;
		x = ( m[0][1] + m[1][0] ) / s;
		y = 0.25 * s;
		z = ( m[1][2] + m[2][1] ) / s;
	} else {
		const double s = sqrt( 1.0 + m[2][2] - m[0][0] - m[1][1] ) * 2.0;
		w = ( m[1][0] - m[0][1] ) / s;
		x = ( m[0][2] + m[2][0] ) / s;
		y = ( m[1][2] + m[2][1] ) / s;
		z = 0.25 * s;
	}

	q[0] = (float)x;
	q[1] = (float)y;
	q[2] = (float)z;
	q[3] = (float)w;
}

void CamPath_QuatToAngles( const float q[4], float angles[3] ) {
	const double len = sqrt( (double)q[0] * q[0] + (double)q[1] * q[1] + (double)q[2] * q[2] + (double)q[3] * q[3] );
	const double x = q[0] / len, y = q[1] / len, z = q[2] / len, w = q[3] / len;
	double m[3][3];

	m[0][0] = 1.0 - 2.0 * ( y * y + z * z );
	m[0][1] = 2.0 * ( x * y - z * w );
	m[0][2] = 2.0 * ( x * z + y * w );
	m[1][0] = 2.0 * ( x * y + z * w );
	m[1][1] = 1.0 - 2.0 * ( x * x + z * z );
	m[1][2] = 2.0 * ( y * z - x * w );
	m[2][0] = 2.0 * ( x * z - y * w );
	m[2][1] = 2.0 * ( y * z + x * w );
	m[2][2] = 1.0 - 2.0 * ( x * x + y * y );

	MatrixToAngles( m, angles );
}

void CamPath_Slerp( const float from[4], const float to[4], float t, float out[4] ) {
	double b[4] = { to[0], to[1], to[2], to[3] };
	double cosom = (double)from[0] * b[0] + (double)from[1] * b[1] + (double)from[2] * b[2] + (double)from[3] * b[3];
	double s0, s1;

	// take the short way around
	if ( cosom < 0.0 ) {
		cosom = -cosom;
		for ( int i = 0; i < 4; i++ ) {
			b[i] = -b[i];
		}
	}

	if ( cosom < 0.9995 ) {
		const double omega = acos( cosom );
		const double sinom = sin( omega );
		s0 = sin( ( 1.0 - t ) * omega ) / sinom;
		s1 = sin( t * omega ) / sinom;
	} else {
		// nearly parallel: a normalized lerp is accurate and stable
		s0 = 1.0 - t;
		s1 = t;
	}

	double len = 0.0, r[4];
	for ( int i = 0; i < 4; i++ ) {
		r[i] = s0 * from[i] + s1 * b[i];
		len += r[i] * r[i];
	}
	len = sqrt( len );
	for ( int i = 0; i < 4; i++ ) {
		out[i] = (float)( r[i] / len );
	}
}

// Quaternions in double precision, x y z w
static void QuatMul( const double a[4], const double b[4], double out[4] ) {
	out[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
	out[1] = a[3] * b[1] + a[1] * b[3] + a[2] * b[0] - a[0] * b[2];
	out[2] = a[3] * b[2] + a[2] * b[3] + a[0] * b[1] - a[1] * b[0];
	out[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
}

// log of the rotation from a to b, the short way: axis times half angle
static void QuatLogBetween( const double a[4], const double b[4], double out[3] ) {
	const double conj[4] = { -a[0], -a[1], -a[2], a[3] };
	double r[4];

	QuatMul( conj, b, r );
	if ( r[3] < 0.0 ) {
		for ( int i = 0; i < 4; i++ ) {
			r[i] = -r[i];
		}
	}
	const double s = sqrt( r[0] * r[0] + r[1] * r[1] + r[2] * r[2] );
	const double k = s > 1e-12 ? atan2( s, r[3] ) / s : 1.0;
	for ( int i = 0; i < 3; i++ ) {
		out[i] = r[i] * k;
	}
}

// a * exp( v )
static void QuatMulExp( const double a[4], const double v[3], double out[4] ) {
	const double angle = sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
	const double k = angle > 1e-12 ? sin( angle ) / angle : 1.0;
	const double e[4] = { v[0] * k, v[1] * k, v[2] * k, cos( angle ) };

	QuatMul( a, e, out );
}

// slerp without taking the short way: the squad terms are already aligned
static void QuatSlerpAligned( const double a[4], const double b[4], double t, double out[4] ) {
	const double cosom = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
	double s0 = 1.0 - t, s1 = t, len = 0.0;

	if ( fabs( cosom ) < 0.9995 ) {
		const double omega = acos( cosom );
		s0 = sin( ( 1.0 - t ) * omega ) / sin( omega );
		s1 = sin( t * omega ) / sin( omega );
	}
	for ( int i = 0; i < 4; i++ ) {
		out[i] = s0 * a[i] + s1 * b[i];
		len += out[i] * out[i];
	}
	len = sqrt( len );
	for ( int i = 0; i < 4; i++ ) {
		out[i] /= len;
	}
}

static void KeyQuat( const camKey_t *key, const double align[4], double q[4] ) {
	float f[4];

	CamPath_AnglesToQuat( key->angles, f );
	for ( int i = 0; i < 4; i++ ) {
		q[i] = f[i];
	}
	if ( align && q[0] * align[0] + q[1] * align[1] + q[2] * align[2] + q[3] * align[3] < 0.0 ) {
		for ( int i = 0; i < 4; i++ ) {
			q[i] = -q[i];
		}
	}
}

/*
Orientation between keys i and i + 1 at u in [0, 1]: a squad whose
tangents in log space are the Catmull-Rom ones of the positions, scaled to
the uneven key spacing. Squad's derivative at u = 0 is log(qa^-1 qb) +
2 log(qa^-1 ca), which gives ca for a wanted tangent; likewise at u = 1.
With evenly spaced keys this is Shoemake's squad.
*/
static void SquadOrientation( const camKey_t *keys, int numKeys, int i, double u, float angles[3] ) {
	const int prev = i > 0 ? i - 1 : i;
	const int next = i + 2 < numKeys ? i + 2 : i + 1;
	double qa[4], qb[4], qp[4], qn[4];

	KeyQuat( &keys[i], nullptr, qa );
	KeyQuat( &keys[i + 1], qa, qb );
	KeyQuat( &keys[prev], qa, qp );
	KeyQuat( &keys[next], qb, qn );

	const double dt = (double)keys[i + 1].time - keys[i].time;
	const double spanA = (double)keys[i + 1].time - keys[prev].time;
	const double spanB = (double)keys[next].time - keys[i].time;
	double forward[3], backward[3], ahead[3], behind[3];

	QuatLogBetween( qa, qb, forward );		// a to b
	QuatLogBetween( qa, qp, backward );		// a to the key before
	QuatLogBetween( qb, qn, ahead );		// b to the key after
	QuatLogBetween( qb, qa, behind );		// b to a

	double toCa[3], toCb[3];
	for ( int k = 0; k < 3; k++ ) {
		const double tangentA = spanA > 0.0 ? ( forward[k] - backward[k] ) / spanA * dt : forward[k];
		const double tangentB = spanB > 0.0 ? ( ahead[k] - behind[k] ) / spanB * dt : -behind[k];

		toCa[k] = ( tangentA - forward[k] ) * 0.5;
		toCb[k] = ( -tangentB - behind[k] ) * 0.5;
	}

	double ca[4], cb[4], outer[4], inner[4], q[4];
	QuatMulExp( qa, toCa, ca );
	QuatMulExp( qb, toCb, cb );
	QuatSlerpAligned( qa, qb, u, outer );
	QuatSlerpAligned( ca, cb, u, inner );
	QuatSlerpAligned( outer, inner, 2.0 * u * ( 1.0 - u ), q );

	const float f[4] = { (float)q[0], (float)q[1], (float)q[2], (float)q[3] };
	CamPath_QuatToAngles( f, angles );
}

// Catmull-Rom tangent at key i for a segment of length dt, scaled to the
// uneven spacing of the keys
static double Tangent( const camKey_t *keys, int numKeys, int i, int axis, double dt ) {
	const int prev = i > 0 ? i - 1 : i;
	const int next = i < numKeys - 1 ? i + 1 : i;
	const double span = (double)keys[next].time - keys[prev].time;

	if ( span <= 0.0 ) {
		return 0.0;
	}
	return ( (double)keys[next].origin[axis] - keys[prev].origin[axis] ) / span * dt;
}

bool CamPath_Evaluate( const camKey_t *keys, int numKeys, double time, camView_t *out ) {
	int i;

	if ( numKeys <= 0 ) {
		return false;
	}

	if ( numKeys == 1 || time <= keys[0].time || time >= keys[numKeys - 1].time ) {
		const camKey_t *k = ( numKeys == 1 || time <= keys[0].time ) ? &keys[0] : &keys[numKeys - 1];

		memcpy( out->origin, k->origin, sizeof( out->origin ) );
		memcpy( out->angles, k->angles, sizeof( out->angles ) );
		out->fov = k->fov;
		out->timescale = k->timescale;
		return true;
	}

	for ( i = 0; i < numKeys - 2 && time >= keys[i + 1].time; i++ ) {
	}

	const camKey_t *a = &keys[i], *b = &keys[i + 1];
	const double dt = (double)b->time - a->time;
	const double u = dt > 0.0 ? ( time - a->time ) / dt : 0.0;
	const double u2 = u * u, u3 = u2 * u;
	const double h00 = 2.0 * u3 - 3.0 * u2 + 1.0;
	const double h10 = u3 - 2.0 * u2 + u;
	const double h01 = -2.0 * u3 + 3.0 * u2;
	const double h11 = u3 - u2;

	for ( int axis = 0; axis < 3; axis++ ) {
		out->origin[axis] = (float)( h00 * a->origin[axis] + h10 * Tangent( keys, numKeys, i, axis, dt ) +
			h01 * b->origin[axis] + h11 * Tangent( keys, numKeys, i + 1, axis, dt ) );
	}

	SquadOrientation( keys, numKeys, i, u, out->angles );

	out->fov = (float)( a->fov + ( b->fov - a->fov ) * u );
	out->timescale = (float)( a->timescale + ( b->timescale - a->timescale ) * u );
	return true;
}

int CamPath_Insert( camKey_t *keys, int numKeys, int maxKeys, const camKey_t *key ) {
	int i;

	for ( i = 0; i < numKeys && keys[i].time < key->time; i++ ) {
	}

	if ( i < numKeys && keys[i].time == key->time ) {
		keys[i] = *key;
		return numKeys;
	}
	if ( numKeys >= maxKeys ) {
		return -1;
	}

	memmove( &keys[i + 1], &keys[i], ( numKeys - i ) * sizeof( keys[0] ) );
	keys[i] = *key;
	return numKeys + 1;
}
