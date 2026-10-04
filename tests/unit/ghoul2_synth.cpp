// ghoul2_synth.cpp - synthetic GLA/GLM files for the FE-16 Ghoul2 reference tests
//
// The files are written byte by byte in the on-disk layout R_LoadMDXA and
// R_LoadMDXM read (renderer/mdx_format.h). Every value comes from integer
// arithmetic, exact or correctly rounded double operations (+ - * / sqrt)
// and float conversions, never from sin, cos or another libm function whose
// last bit depends on the platform, and each random draw is a statement of
// its own (argument evaluation order is unspecified), so the bytes are the
// same everywhere; test_ghoul2 checks their hashes first.

#include "ghoul2_harness.h"

#include <math.h>
#include <map>

namespace g2t {

const char *const kCharGLA = "models/test/g2char.gla";
const char *const kCharGLM = "models/test/g2char.glm";
const char *const kPropGLA = "models/test/g2prop.gla";
const char *const kPropGLM = "models/test/g2prop.glm";

namespace {

// xorshift32: the same sequence on every platform
class Rng {
public:
	explicit Rng( uint32_t seed ) : s( seed ? seed : 0x9e3779b9u ) {}
	uint32_t Next( void ) {
		s ^= s << 13;
		s ^= s >> 17;
		s ^= s << 5;
		return s;
	}
	// integer in [lo, hi]
	int Range( int lo, int hi ) {
		return lo + (int)( Next() % (uint32_t)( hi - lo + 1 ) );
	}
	// multiple of 1/1024 in [-1, 1], exact in a double
	double Sym( void ) {
		return Range( -1024, 1024 ) / 1024.0;
	}
private:
	uint32_t s;
};

// little-endian writer
class Writer {
public:
	Bytes data;
	int Pos( void ) const { return (int)data.size(); }
	void U8( unsigned v ) { data.push_back( (unsigned char)( v & 0xff ) ); }
	void U16( unsigned v ) { U8( v ); U8( v >> 8 ); }
	void U32( uint32_t v ) { U16( v & 0xffff ); U16( v >> 16 ); }
	void I32( int v ) { U32( (uint32_t)v ); }
	void F32( float f ) {
		uint32_t u;
		memcpy( &u, &f, sizeof( u ) );
		U32( u );
	}
	// fixed-size, zero padded string
	void Name( const char *s, int size ) {
		const int len = (int)strlen( s );
		for ( int i = 0; i < size; i++ ) {
			U8( i < len && i < size - 1 ? (unsigned char)s[i] : 0 );
		}
	}
	void Zeros( int count ) {
		while ( count-- > 0 ) {
			U8( 0 );
		}
	}
	void Align4( void ) {
		while ( data.size() & 3 ) {
			U8( 0 );
		}
	}
	void PatchI32( int at, int v ) {
		const uint32_t u = (uint32_t)v;
		for ( int i = 0; i < 4; i++ ) {
			data[at + i] = (unsigned char)( ( u >> ( 8 * i ) ) & 0xff );
		}
	}
};

struct Quat {
	double w, x, y, z;
};

Quat QNormalize( double w, double x, double y, double z ) {
	const double n = sqrt( w * w + x * x + y * y + z * z );
	Quat q = { w / n, x / n, y / n, z / n };
	return q;
}

Quat QMul( const Quat &a, const Quat &b ) {
	return QNormalize(
		a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
		a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
		a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
		a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w );
}

Quat QConj( const Quat &q ) {
	Quat c = { q.w, -q.x, -q.y, -q.z };
	return c;
}

// rotation matrix of a unit quaternion, in the convention of MC_UnCompressQuat
// (column vectors: v' = m v)
void QMatrix( const Quat &q, double m[3][3] ) {
	m[0][0] = 1.0 - 2.0 * ( q.y * q.y + q.z * q.z );
	m[0][1] = 2.0 * ( q.x * q.y - q.w * q.z );
	m[0][2] = 2.0 * ( q.x * q.z + q.w * q.y );
	m[1][0] = 2.0 * ( q.x * q.y + q.w * q.z );
	m[1][1] = 1.0 - 2.0 * ( q.x * q.x + q.z * q.z );
	m[1][2] = 2.0 * ( q.y * q.z - q.w * q.x );
	m[2][0] = 2.0 * ( q.x * q.z - q.w * q.y );
	m[2][1] = 2.0 * ( q.y * q.z + q.w * q.x );
	m[2][2] = 1.0 - 2.0 * ( q.x * q.x + q.y * q.y );
}

// triangle wave in [-1, 1] of period 'period' frames
double Tri( int frame, int period ) {
	const int u = ( ( frame % period ) + period ) % period;
	const double t = 4.0 * u / period;
	return t < 2.0 ? t - 1.0 : 3.0 - t;
}

unsigned Quantize( double v, double offset, double scale ) {
	double s = floor( ( v + offset ) * scale + 0.5 );
	if ( s < 0.0 ) {
		s = 0.0;
	} else if ( s > 65535.0 ) {
		s = 65535.0;
	}
	return (unsigned)s;
}

//
// skeletons
//

struct BoneDef {
	const char	*name;
	int			parent;
	unsigned	flags;
	double		joint[3];	// model space position in the base pose
	double		amplitude;	// of the animation, 0 for a bone that never moves
};

const BoneDef kCharBoneDefs[kCharBones] = {
	{ "model_root",		-1,	0,						{ 0, 0, 0 },	0.10 },
	{ "pelvis",			0,	0,						{ 0, 0, 38 },	0.20 },
	{ "Motion",			0,	0,						{ 0, 0, 0 },	0.00 },	// no surface uses it
	{ "lower_lumbar",	1,	0,						{ 0, 0, 42 },	0.15 },
	{ "upper_lumbar",	3,	0,						{ 0, 0, 47 },	0.15 },
	{ "thoracic",		4,	0,						{ 0, 0, 52 },	0.15 },
	{ "cervical",		5,	0,						{ 0, 0, 58 },	0.20 },
	{ "cranium",		6,	0,						{ 1, 0, 62 },	0.25 },
	{ "ceyebrow",		7,	G2BONEFLAG_ALWAYSXFORM,	{ 4, 0, 66 },	0.00 },	// no surface, transformed for its flag
	{ "face",			7,	0,						{ 5, 0, 63 },	0.10 },	// no surface, no flag: never transformed
	{ "rclavical",		5,	0,						{ 0, -3, 56 },	0.10 },
	{ "rhumerus",		10,	0,						{ 0, -8, 55 },	0.35 },
	{ "rradius",		11,	0,						{ 0, -9, 45 },	0.35 },
	{ "rhand",			12,	0,						{ 2, -9, 36 },	0.30 },
	{ "r_d1_j1",		13,	0,						{ 4, -9, 33 },	0.30 },
	{ "rhang_tag_bone",	13,	G2BONEFLAG_ALWAYSXFORM,	{ 3, -9, 34 },	0.00 },
	{ "lclavical",		5,	0,						{ 0, 3, 56 },	0.10 },
	{ "lhumerus",		16,	0,						{ 0, 8, 55 },	0.35 },
	{ "lradius",		17,	0,						{ 0, 9, 45 },	0.35 },
	{ "lhand",			18,	0,						{ 2, 9, 36 },	0.30 },
	{ "rfemurYZ",		1,	0,						{ 0, -4, 36 },	0.30 },
	{ "rtibia",			20,	0,						{ 1, -4, 20 },	0.30 },
	{ "rtalus",			21,	0,						{ 0, -4, 4 },	0.20 },
	{ "lfemurYZ",		1,	0,						{ 0, 4, 36 },	0.30 },
	{ "ltibia",			23,	0,						{ 1, 4, 20 },	0.30 },
	{ "ltalus",			24,	0,						{ 0, 4, 4 },	0.20 },
};

enum { kPropBones = 3, kPropFrames = 8 };

const BoneDef kPropBoneDefs[kPropBones] = {
	{ "prop_root",		-1,	0,	{ 0, 0, 0 },	0.10 },
	{ "prop_hilt",		0,	0,	{ 0, 0, 4 },	0.20 },
	{ "prop_tip",		1,	0,	{ 0, 0, 32 },	0.05 },
};

struct CompBone {
	unsigned short v[7];
	bool operator<( const CompBone &o ) const {
		return memcmp( v, o.v, sizeof( v ) ) < 0;
	}
};

Bytes BuildGLA( const char *name, const BoneDef *defs, int numBones, int numFrames, uint32_t seed ) {
	Rng rng( seed );

	// base pose rotations and per-bone animation parameters
	std::vector<Quat> baseRot( numBones );
	std::vector<double> amp( numBones * 3 );
	std::vector<int> period( numBones ), phase( numBones );
	static const int periods[4] = { 12, 16, 24, 48 };
	for ( int b = 0; b < numBones; b++ ) {
		// one draw per statement: the order of evaluation of function
		// arguments differs between compilers and even build types
		const double x = 0.25 * rng.Sym();
		const double y = 0.25 * rng.Sym();
		const double z = 0.25 * rng.Sym();
		baseRot[b] = QNormalize( 1.0, x, y, z );
		for ( int a = 0; a < 3; a++ ) {
			amp[b * 3 + a] = defs[b].amplitude * ( 0.5 + 0.5 * fabs( rng.Sym() ) );
		}
		period[b] = periods[rng.Range( 0, 3 )];
		phase[b] = rng.Range( 0, 47 );
	}

	// each frame of each bone: a rotation about the bone's joint in model
	// space (base * local * base^-1), so the hierarchy composes like a
	// skeleton; the root also drifts a little
	std::vector<CompBone> frames( numFrames * numBones );
	std::map<CompBone, int> poolIndex;
	std::vector<CompBone> pool;
	for ( int f = 0; f < numFrames; f++ ) {
		for ( int b = 0; b < numBones; b++ ) {
			const int p = period[b];
			const int t = f + phase[b];
			const Quat local = QNormalize( 1.0,
				amp[b * 3 + 0] * Tri( t, p ),
				amp[b * 3 + 1] * Tri( t + p / 3, p ),
				amp[b * 3 + 2] * Tri( t + ( 2 * p ) / 3, p ) );
			Quat q = QMul( QMul( baseRot[b], local ), QConj( baseRot[b] ) );
			if ( q.w < 0.0 ) {
				q.w = -q.w; q.x = -q.x; q.y = -q.y; q.z = -q.z;
			}

			double m[3][3];
			QMatrix( q, m );
			const double *j = defs[b].joint;
			double trans[3];
			for ( int i = 0; i < 3; i++ ) {
				trans[i] = j[i] - ( m[i][0] * j[0] + m[i][1] * j[1] + m[i][2] * j[2] );
			}
			if ( defs[b].parent < 0 && defs[b].amplitude > 0.0 ) {
				trans[0] += 1.5 * Tri( t, 24 );
				trans[1] += 0.75 * Tri( t + 8, 24 );
				trans[2] += 1.0 * Tri( t + 16, 24 );
			}

			CompBone cb;
			cb.v[0] = (unsigned short)Quantize( q.w, 2.0, 16383.0 );
			cb.v[1] = (unsigned short)Quantize( q.x, 2.0, 16383.0 );
			cb.v[2] = (unsigned short)Quantize( q.y, 2.0, 16383.0 );
			cb.v[3] = (unsigned short)Quantize( q.z, 2.0, 16383.0 );
			cb.v[4] = (unsigned short)Quantize( trans[0], 512.0, 64.0 );
			cb.v[5] = (unsigned short)Quantize( trans[1], 512.0, 64.0 );
			cb.v[6] = (unsigned short)Quantize( trans[2], 512.0, 64.0 );
			frames[f * numBones + b] = cb;

			// the pool shares identical bones, like Carcass does
			if ( poolIndex.find( cb ) == poolIndex.end() ) {
				poolIndex[cb] = (int)pool.size();
				pool.push_back( cb );
			}
		}
	}

	Writer w;
	w.I32( MDXA_IDENT );
	w.I32( MDXA_VERSION );
	char animName[MAX_QPATH];
	COM_StripExtension( name, animName, sizeof( animName ) );
	w.Name( animName, MAX_QPATH );
	w.F32( 1.0f );					// fScale
	w.I32( numFrames );
	const int ofsFramesAt = w.Pos();
	w.I32( 0 );
	w.I32( numBones );
	const int ofsPoolAt = w.Pos();
	w.I32( 0 );
	w.I32( (int)sizeof( mdxaHeader_t ) );	// ofsSkel
	const int ofsEndAt = w.Pos();
	w.I32( 0 );

	// skeleton: offsets relative to the offset table, then the bones
	const int offsetsAt = w.Pos();
	w.Zeros( 4 * numBones );
	for ( int b = 0; b < numBones; b++ ) {
		w.PatchI32( offsetsAt + 4 * b, w.Pos() - offsetsAt );
		w.Name( defs[b].name, MAX_QPATH );
		w.U32( defs[b].flags );
		w.I32( defs[b].parent );

		double m[3][3];
		QMatrix( baseRot[b], m );
		const double *j = defs[b].joint;
		for ( int r = 0; r < 3; r++ ) {	// BasePoseMat
			w.F32( (float)m[r][0] );
			w.F32( (float)m[r][1] );
			w.F32( (float)m[r][2] );
			w.F32( (float)j[r] );
		}
		for ( int r = 0; r < 3; r++ ) {	// BasePoseMatInv: transpose, -R^T p
			w.F32( (float)m[0][r] );
			w.F32( (float)m[1][r] );
			w.F32( (float)m[2][r] );
			w.F32( (float)-( m[0][r] * j[0] + m[1][r] * j[1] + m[2][r] * j[2] ) );
		}

		int numChildren = 0;
		for ( int c = 0; c < numBones; c++ ) {
			if ( defs[c].parent == b ) {
				numChildren++;
			}
		}
		w.I32( numChildren );
		for ( int c = 0; c < numBones; c++ ) {
			if ( defs[c].parent == b ) {
				w.I32( c );
			}
		}
	}

	// frames: a 24-bit pool index per bone
	w.PatchI32( ofsFramesAt, w.Pos() );
	for ( size_t i = 0; i < frames.size(); i++ ) {
		const int index = poolIndex[frames[i]];
		w.U8( index );
		w.U8( index >> 8 );
		w.U8( index >> 16 );
	}
	w.Align4();

	// the compressed bone pool
	w.PatchI32( ofsPoolAt, w.Pos() );
	for ( size_t i = 0; i < pool.size(); i++ ) {
		for ( int k = 0; k < 7; k++ ) {
			w.U16( pool[i].v[k] );
		}
	}
	w.Align4();
	w.PatchI32( ofsEndAt, w.Pos() );
	return w.data;
}

//
// meshes
//

enum { kMaxSurfBones = 24 };

struct SurfDef {
	const char	*name;
	unsigned	flags;
	int			parent;
	int			verts;			// LOD 0; tag surfaces always have 3
	int			emptyFromLod;	// tag surfaces: first LOD where the surface has no triangle
	int			bones[kMaxSurfBones + 1];	// -1 terminated
};

const SurfDef kCharSurfDefs[kCharSurfaces] = {
	{ "hips",			0,						-1,	60,		0,	{ 1, 3, 20, 23, -1 } },
	{ "torso",			0,						0,	90,		0,	{ 3, 4, 5, 6, 10, 16, -1 } },
	{ "head",			0,						1,	48,		0,	{ 6, 7, -1 } },
	{ "head_cap_torso_off", G2SURFACEFLAG_OFF,	2,	12,		0,	{ 6, 7, -1 } },
	{ "r_arm",			0,						1,	64,		0,	{ 10, 11, 12, 13, 14, -1 } },
	{ "*r_hand",		G2SURFACEFLAG_ISBOLT,	4,	3,		2,	{ 13, -1 } },
	{ "l_arm",			0,						1,	56,		0,	{ 16, 17, 18, 19, -1 } },
	{ "*l_hand",		G2SURFACEFLAG_ISBOLT,	6,	3,		99,	{ 19, -1 } },
	{ "r_leg",			0,						0,	52,		0,	{ 20, 21, 22, -1 } },
	{ "l_leg",			0,						0,	52,		0,	{ 23, 24, 25, -1 } },
	// many bone references and 4-weight vertexes (the SSE2 kernel caches 32)
	{ "cloak",			0,						1,	120,	0,	{ 0, 1, 3, 4, 5, 6, 7, 10, 11, 12, 13, 14, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1 } },
	{ "torso_cap_head_off", G2SURFACEFLAG_OFF,	1,	12,		0,	{ 5, 6, -1 } },
};

enum { kPropSurfaces = 3, kPropLods = 2 };

const SurfDef kPropSurfDefs[kPropSurfaces] = {
	{ "blade",			0,						-1,	40,		0,	{ 0, 1, 2, -1 } },
	{ "*flash",			G2SURFACEFLAG_ISBOLT,	0,	3,		99,	{ 2, -1 } },
	{ "hilt",			0,						0,	24,		0,	{ 0, 1, -1 } },
};

int SurfBoneCount( const SurfDef &s ) {
	int n = 0;
	while ( n < kMaxSurfBones && s.bones[n] >= 0 ) {
		n++;
	}
	return n;
}

int LodVerts( const SurfDef &s, int lod ) {
	if ( s.name[0] == '*' ) {
		return lod >= s.emptyFromLod ? 0 : 3;
	}
	const int v = s.verts * ( 3 - lod ) / 3;
	return v < 3 ? 3 : v;
}

void WriteVertex( Writer &w, const double pos[3], const double normal[3], int numWeights, const int *refIndex, const int *weight ) {
	w.F32( (float)normal[0] );
	w.F32( (float)normal[1] );
	w.F32( (float)normal[2] );
	w.F32( (float)pos[0] );
	w.F32( (float)pos[1] );
	w.F32( (float)pos[2] );

	// 31-30: weights - 1; 5 bits per bone reference index; the two high
	// bits of each 10-bit weight at 20 + 2k; the low 8 bits in BoneWeightings
	uint32_t packed = (uint32_t)( numWeights - 1 ) << 30;
	for ( int k = 0; k < numWeights; k++ ) {
		packed |= (uint32_t)refIndex[k] << ( iG2_BITS_PER_BONEREF * k );
		packed |= (uint32_t)( ( weight[k] >> 8 ) & 3 ) << ( 20 + 2 * k );
	}
	w.U32( packed );
	for ( int k = 0; k < iMAX_G2_BONEWEIGHTS_PER_VERT; k++ ) {
		w.U8( k < numWeights ? weight[k] & 0xff : 0 );
	}
}

void WriteSurface( Writer &w, int surfaceIndex, const SurfDef &s, int lod, const BoneDef *bones, uint32_t seed ) {
	Rng rng( seed ^ ( (uint32_t)surfaceIndex * 977u + (uint32_t)lod * 131u + 7u ) );
	const int surfStart = w.Pos();
	const int numRefs = SurfBoneCount( s );
	const int numVerts = LodVerts( s, lod );
	const bool tag = s.name[0] == '*';
	const int numTris = tag ? ( numVerts ? 1 : 0 ) : numVerts;

	w.I32( 0 );					// ident (SF_MDX once loaded)
	w.I32( surfaceIndex );		// thisSurfaceIndex
	w.I32( -surfStart );		// ofsHeader
	w.I32( numVerts );
	const int ofsVertsAt = w.Pos();
	w.I32( 0 );
	w.I32( numTris );
	const int ofsTrisAt = w.Pos();
	w.I32( 0 );
	w.I32( numRefs );
	const int ofsRefsAt = w.Pos();
	w.I32( 0 );
	const int ofsEndAt = w.Pos();
	w.I32( 0 );

	w.PatchI32( ofsRefsAt, w.Pos() - surfStart );
	for ( int i = 0; i < numRefs; i++ ) {
		w.I32( s.bones[i] );
	}

	w.PatchI32( ofsTrisAt, w.Pos() - surfStart );
	if ( tag ) {
		if ( numTris ) {
			w.I32( 0 );
			w.I32( 1 );
			w.I32( 2 );
		}
	} else {
		for ( int t = 0; t < numTris; t++ ) {
			const int a = rng.Range( 0, numVerts - 1 );
			int b = rng.Range( 0, numVerts - 2 );
			b += ( b >= a ) ? 1 : 0;
			int c;
			do {
				c = rng.Range( 0, numVerts - 1 );
			} while ( c == a || c == b );
			w.I32( a );
			w.I32( b );
			w.I32( c );
		}
	}

	w.PatchI32( ofsVertsAt, w.Pos() - surfStart );
	std::vector<double> texCoords;
	for ( int v = 0; v < numVerts; v++ ) {
		double pos[3], normal[3];
		int refIndex[iMAX_G2_BONEWEIGHTS_PER_VERT], weight[iMAX_G2_BONEWEIGHTS_PER_VERT];
		int numWeights;

		if ( tag ) {
			// a tag triangle, sides 0 (v0 v1) longest and 2 (v2 v0)
			// shortest as G2_ProcessSurfaceBolt expects, origin at v2;
			// the same in every LOD
			static const double offsets[3][3] = { { 0, 0, -2 }, { 6, 0, -2 }, { 1, 2, -2 } };
			const double *j = bones[s.bones[0]].joint;
			for ( int i = 0; i < 3; i++ ) {
				pos[i] = j[i] + offsets[v][i];
			}
			normal[0] = 0.0;
			normal[1] = 0.0;
			normal[2] = 1.0;
			numWeights = 1;
			refIndex[0] = 0;
			weight[0] = 1023;
		} else {
			numWeights = 1 + rng.Range( 0, 3 );
			if ( numWeights > numRefs ) {
				numWeights = numRefs;
			}
			for ( int k = 0; k < numWeights; k++ ) {
				bool unique;
				do {
					refIndex[k] = rng.Range( 0, numRefs - 1 );
					unique = true;
					for ( int m = 0; m < k; m++ ) {
						unique = unique && refIndex[m] != refIndex[k];
					}
				} while ( !unique );
			}
			// explicit 10-bit weights; the last one is 1 - the others
			int remaining = 1023;
			for ( int k = 0; k < numWeights - 1; k++ ) {
				int hi = remaining - 32 * ( numWeights - 1 - k );
				if ( hi > 800 ) {
					hi = 800;
				}
				weight[k] = rng.Range( 32, hi < 32 ? 32 : hi );
				remaining -= weight[k];
			}
			weight[numWeights - 1] = remaining;

			const double *j = bones[s.bones[refIndex[0]]].joint;
			for ( int i = 0; i < 3; i++ ) {
				pos[i] = j[i] + rng.Range( -320, 320 ) / 64.0;
			}
			double len2;
			do {
				normal[0] = rng.Sym();
				normal[1] = rng.Sym();
				normal[2] = rng.Sym();
				len2 = normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2];
			} while ( len2 < 0.01 );
			const double len = sqrt( len2 );
			normal[0] /= len;
			normal[1] /= len;
			normal[2] /= len;
		}
		WriteVertex( w, pos, normal, numWeights, refIndex, weight );
		texCoords.push_back( rng.Range( 0, 1023 ) / 1024.0 );
		texCoords.push_back( rng.Range( 0, 1023 ) / 1024.0 );
	}
	for ( size_t i = 0; i < texCoords.size(); i++ ) {
		w.F32( (float)texCoords[i] );
	}

	w.PatchI32( ofsEndAt, w.Pos() - surfStart );
}

Bytes BuildGLM( const char *name, const char *glaName, const BoneDef *bones, int numBones,
				const SurfDef *surfs, int numSurfs, int numLods, uint32_t seed ) {
	Writer w;
	w.I32( MDXM_IDENT );
	w.I32( MDXM_VERSION );
	w.Name( name, MAX_QPATH );
	char animName[MAX_QPATH];
	COM_StripExtension( glaName, animName, sizeof( animName ) );
	w.Name( animName, MAX_QPATH );
	w.I32( 0 );						// animIndex
	w.I32( numBones );
	w.I32( numLods );
	const int ofsLodsAt = w.Pos();
	w.I32( 0 );
	w.I32( numSurfs );
	const int ofsHierarchyAt = w.Pos();
	w.I32( 0 );
	const int ofsEndAt = w.Pos();
	w.I32( 0 );

	// surface hierarchy: offsets relative to the offset table, then the
	// entries in surface order
	const int offsetsAt = w.Pos();
	w.Zeros( 4 * numSurfs );
	w.PatchI32( ofsHierarchyAt, w.Pos() );
	for ( int s = 0; s < numSurfs; s++ ) {
		w.PatchI32( offsetsAt + 4 * s, w.Pos() - offsetsAt );
		w.Name( surfs[s].name, MAX_QPATH );
		w.U32( surfs[s].flags );
		char shader[MAX_QPATH];
		Com_sprintf( shader, sizeof( shader ), "%s/%s", animName, surfs[s].name[0] == '*' ? "tag" : surfs[s].name );
		w.Name( shader, MAX_QPATH );
		w.I32( 0 );					// shaderIndex
		w.I32( surfs[s].parent );
		int numChildren = 0;
		for ( int c = 0; c < numSurfs; c++ ) {
			if ( surfs[c].parent == s ) {
				numChildren++;
			}
		}
		w.I32( numChildren );
		for ( int c = 0; c < numSurfs; c++ ) {
			if ( surfs[c].parent == s ) {
				w.I32( c );
			}
		}
	}

	// LODs: ofsEnd, surface offsets relative to their table, the surfaces
	w.PatchI32( ofsLodsAt, w.Pos() );
	for ( int lod = 0; lod < numLods; lod++ ) {
		const int lodStart = w.Pos();
		w.I32( 0 );
		const int surfOffsetsAt = w.Pos();
		w.Zeros( 4 * numSurfs );
		for ( int s = 0; s < numSurfs; s++ ) {
			w.PatchI32( surfOffsetsAt + 4 * s, w.Pos() - surfOffsetsAt );
			WriteSurface( w, s, surfs[s], lod, bones, seed );
		}
		w.PatchI32( lodStart, w.Pos() - lodStart );
	}
	w.PatchI32( ofsEndAt, w.Pos() );
	(void)numBones;
	return w.data;
}

} // namespace

Bytes BuildCharGLA( void ) {
	return BuildGLA( kCharGLA, kCharBoneDefs, kCharBones, kCharFrames, 0x6a09e667u );
}

Bytes BuildCharGLM( void ) {
	return BuildGLM( kCharGLM, kCharGLA, kCharBoneDefs, kCharBones, kCharSurfDefs, kCharSurfaces, kCharLods, 0xbb67ae85u );
}

Bytes BuildPropGLA( void ) {
	return BuildGLA( kPropGLA, kPropBoneDefs, kPropBones, kPropFrames, 0x3c6ef372u );
}

Bytes BuildPropGLM( void ) {
	return BuildGLM( kPropGLM, kPropGLA, kPropBoneDefs, kPropBones, kPropSurfDefs, kPropSurfaces, kPropLods, 0xa54ff53au );
}

void AddSyntheticModels( void ) {
	AddFile( kCharGLA, BuildCharGLA() );
	AddFile( kCharGLM, BuildCharGLM() );
	AddFile( kPropGLA, BuildPropGLA() );
	AddFile( kPropGLM, BuildPropGLM() );
}

} // namespace g2t
