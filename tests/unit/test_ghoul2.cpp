// test_ghoul2.cpp - FE-16: bit-exact reference tests of the Ghoul2 code
//
// What runs is the engine's own code (tr_ghoul2.cpp, ghoul2/*.cpp, matcomp.c,
// q_math.cpp), compiled with the engine's definitions, through the API the
// game and cgame modules call, on synthetic GLA/GLM models (ghoul2_synth.cpp)
// and, when the retail assets0.pk3 is found, on Kyle. Each scenario hashes
// what it produced (bone matrices and their smoothing time stamps, bolt
// matrices, the draw surfaces of the frame, skinned positions and normals,
// collision records and vertexes) and compares the hash with a reference
// value recorded from the code before any optimization: one bit off fails.
//
// For the Ghoul2 optimizations (FE-3 caches, FE-4 SIMD, FE-5 skinning once
// per frame):
//  - every reference value must hold, on x86-64 and, for portable code, on
//    ARM64 too (it checks the scalar skinning values);
//  - G2_DUMP_DIR=<dir> writes every hashed value to <dir>/<name>.txt: dump
//    before and after a change and diff the folders to find what moved;
//  - only a change meant to alter results may update a value, and its commit
//    says why; G2_GOLDEN_PRINT=1 prints the table below with the new values.
//
// The skinning values come in two sets: "skin/scalar" (G2_SkinVertexes, the
// portable loop, checked everywhere) and "skin/sse2" (G2_SkinVertexesSSE2,
// what RB_SurfaceGhoul runs on x86, which associates the sums differently).
// RB_SurfaceGhoul's own output must match the kernel it uses, bit for bit.

#include <gtest/gtest.h>
#include "ghoul2_harness.h"

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>

int G2_DecideTraceLod( CGhoul2Info &ghoul2, int useLod, model_t *mod );

namespace {

//
// reference values
//

struct Golden {
	const char			*name;
	unsigned long long	value;
};

// Recorded on x86-64 (MSVC 2022 and the code before any Ghoul2
// optimization); "skin/sse2" values apply to x86 only, every other value
// to every platform
const Golden kGoldens[] = {
	// the synthetic files: if these differ, the generator is not portable
	{ "files/models/test/g2char.gla", 0xe7f5ae0f6b9d7e39ULL },
	{ "files/models/test/g2char.glm", 0xc563ba6500418b75ULL },
	{ "files/models/test/g2prop.gla", 0x6f1afabeffa018e8ULL },
	{ "files/models/test/g2prop.glm", 0x431cbbfd42db9395ULL },

	// kernels
	{ "kernel/multiply_3x4", 0xceb841ad22f894c9ULL },
	{ "kernel/uncompress", 0xf77fbcbcdd1ccacaULL },
	{ "kernel/skin/scalar", 0x1e1ae4a8c884527eULL },
	{ "kernel/skin/sse2", 0xa0bae6b9ce42025bULL },

	// skeletons on the server (no bone smoothing), bolts, attached model, surfaces
	{ "rest/bones", 0x834e4832bd118bafULL },
	{ "loop/bones", 0xbb7d3c77eac3fb6dULL },
	{ "blend/bones", 0x68c857f3185b21beULL },
	{ "reverse/bones", 0x244c0370d1c68ab1ULL },
	{ "angles/bones", 0x862b9772241f4bfcULL },
	{ "bolts/bones", 0x5d375cf1cd705edcULL },
	{ "bolts/bolts", 0x60455e62edb6030bULL },
	{ "neworigin/bones", 0xb5ae1d0890ed160aULL },
	{ "neworigin/bolts", 0x54fb85c03def5c66ULL },
	{ "neworigin/render/draw", 0x7a645b53d6532329ULL },
	{ "neworigin/render/skin/scalar", 0xd61627ac047fad50ULL },
	{ "neworigin/render/skin/sse2", 0x64b3fcd8eb332e2cULL },
	{ "neworigin/render/bones", 0x1624939e299b5eefULL },
	{ "surfaces/bones", 0x2ba6fad17da0ab21ULL },
	{ "surfaces/bolts", 0x4df7e7d83e736dc5ULL },

	// client: bone smoothing and unsquash, the render path
	{ "smooth/bones", 0x62ff5f4cc8d09cdaULL },
	{ "smooth/bolts", 0xec8fda0380b18d06ULL },
	{ "render/lod0/draw", 0xd506880ebf11a810ULL },
	{ "render/lod0/skin/scalar", 0x4186f935201c0b03ULL },
	{ "render/lod0/skin/sse2", 0x237fcf3e36d68f5bULL },
	{ "render/lod0/bones", 0x1470c532cad8c4c3ULL },
	{ "render/lod0/bolts", 0x10e439e199692941ULL },
	{ "render/lod1/draw", 0x6db2fa9e01a5485cULL },
	{ "render/lod1/skin/scalar", 0x85b9567e37174818ULL },
	{ "render/lod1/skin/sse2", 0xe2f3fa17b2e51022ULL },
	{ "render/lod1/bones", 0x1d7650f95877181dULL },
	{ "render/lod1/bolts", 0x0048388c9109ed9fULL },
	{ "render/lod2/draw", 0xf33038569af9f8c4ULL },
	{ "render/lod2/skin/scalar", 0x84c67c4c9f76192cULL },
	{ "render/lod2/skin/sse2", 0x47fa69de0ecd05ebULL },
	{ "render/lod2/bones", 0x10fc6bd56c40067dULL },
	{ "render/lod2/bolts", 0x29c7e438888fccd9ULL },
	{ "render/variants/draw", 0xaf8acd1dfe71c64dULL },
	{ "render/variants/skin/scalar", 0x57fc183821d95989ULL },
	{ "render/variants/skin/sse2", 0x18e693d9d01e6a6dULL },

	// collision
	{ "collision", 0x24adb185bddd584fULL },

	// Kyle from the retail assets0.pk3 (checked when its files match)
	{ "retail/files/models/players/kyle/model.glm", 0x3a2a9e659aab99d7ULL },
	{ "retail/files/models/players/_humanoid/_humanoid.gla", 0xcb967a89b10ab841ULL },
	{ "retail/bones", 0x3574c7b137ce3869ULL },
	{ "retail/bolts", 0x9c061a7ed3d221b6ULL },
	{ "retail/render/lod0/draw", 0x971aa6745dedd5acULL },
	{ "retail/render/lod0/skin/scalar", 0xe8259014737824b3ULL },
	{ "retail/render/lod0/skin/sse2", 0xf14101cbd220b449ULL },
	{ "retail/render/lod0/bones", 0x844301a3bfb94d44ULL },
	{ "retail/render/lod0/bolts", 0x0568cbfda3fa130bULL },
	{ "retail/render/lod1/draw", 0xf8dab51a6e4b2589ULL },
	{ "retail/render/lod1/skin/scalar", 0xe4ebf906146b1724ULL },
	{ "retail/render/lod1/skin/sse2", 0x7b3cda51c2b4033eULL },
	{ "retail/render/lod1/bones", 0xac3dfb05ffcc5d3cULL },
	{ "retail/render/lod1/bolts", 0x3621b046025187d2ULL },
	{ "retail/render/lod2/draw", 0xfa56455739d44ff8ULL },
	{ "retail/render/lod2/skin/scalar", 0x3e05a769a3469a92ULL },
	{ "retail/render/lod2/skin/sse2", 0x2dbdf116c9ecd474ULL },
	{ "retail/render/lod2/bones", 0x5c9c271ab6c314c6ULL },
	{ "retail/render/lod2/bolts", 0xef89d9a9dc02ab8bULL },
	{ "retail/render/lod3/draw", 0x9cf2fcb3b7a12820ULL },
	{ "retail/render/lod3/skin/scalar", 0x45540c68c0a5d21aULL },
	{ "retail/render/lod3/skin/sse2", 0x31c2b8b42409ae0eULL },
	{ "retail/render/lod3/bones", 0x5af8cbc39f4e5a1cULL },
	{ "retail/render/lod3/bolts", 0x0107a2c85052bf1cULL },
	{ "retail/collision", 0x85185fe69ef27398ULL },
};

bool PrintGoldens( void ) {
	const char *v = getenv( "G2_GOLDEN_PRINT" );
	return v && v[0] && v[0] != '0';
}

const Golden *FindGolden( const std::string &name ) {
	for ( size_t i = 0; i < sizeof( kGoldens ) / sizeof( kGoldens[0] ); i++ ) {
		if ( name == kGoldens[i].name ) {
			return &kGoldens[i];
		}
	}
	return NULL;
}

void ExpectGolden( const g2t::Digest &d ) {
	const unsigned long long value = d.Value();
	if ( PrintGoldens() ) {
		printf( "\t{ \"%s\", 0x%016llxULL },\n", d.Name().c_str(), value );
	}
	const Golden *g = FindGolden( d.Name() );
	if ( !g ) {
		ADD_FAILURE() << "no reference value for \"" << d.Name() << "\": add\n"
			<< "\t{ \"" << d.Name() << "\", 0x" << std::hex << value << "ULL },";
		return;
	}
	EXPECT_EQ( g->value, value )
		<< "\"" << d.Name() << "\" changed: the Ghoul2 results are no longer bit-exact.\n"
		<< "Dump the values with G2_DUMP_DIR=<dir> before and after the change and diff them.\n"
		<< "Only a change meant to alter the results may update the value (G2_GOLDEN_PRINT=1).";
}

//
// helpers
//

const vec3_t kNoAngles = { 0, 0, 0 };
const vec3_t kNoScale = { 0, 0, 0 };
const int kT0 = 10000;

void HashFile( g2t::Digest &d, const char *name ) {
	const g2t::Bytes *file = g2t::FindFile( name );
	ASSERT_TRUE( file != NULL ) << name;
	d.Int( (int)file->size() );
	for ( size_t i = 0; i < file->size(); i += 4 ) {
		uint32_t w = 0;
		for ( size_t k = 0; k < 4 && i + k < file->size(); k++ ) {
			w |= (uint32_t)( *file )[i + k] << ( 8 * k );
		}
		d.Int( (int)w );
	}
}

#if id386 || idx64
#define G2_PLATFORM_SKIN G2_SkinVertexesSSE2	// what RB_SurfaceGhoul runs
#else
#define G2_PLATFORM_SKIN G2_SkinVertexes
#endif

void HashSkinned( g2t::Digest &d, const vec4_t *xyz, const vec4_t *normal, int numVerts ) {
	for ( int v = 0; v < numVerts; v++ ) {
		d.Floats( xyz[v], 3 );
		d.Floats( normal[v], 3 );
	}
}

bool SameVertexes( const vec4_t *a, const vec4_t *b, int numVerts ) {
	for ( int v = 0; v < numVerts; v++ ) {
		if ( memcmp( a[v], b[v], 3 * sizeof( float ) ) ) {
			return false;
		}
	}
	return true;
}

// The model whose skeleton a draw surface uses
int ModelOfBoneList( CGhoul2Info_v &ghoul2, const void *boneList ) {
	for ( size_t i = 0; i < ghoul2.size(); i++ ) {
		if ( &ghoul2[i].mTempBoneList == boneList ) {
			return (int)i;
		}
	}
	return -1;
}

bool RecordLess( const CollisionRecord_t *a, const CollisionRecord_t *b ) {
	if ( a->mDistance != b->mDistance ) {
		return a->mDistance < b->mDistance;
	}
	if ( a->mModelIndex != b->mModelIndex ) {
		return a->mModelIndex < b->mModelIndex;
	}
	if ( a->mSurfaceIndex != b->mSurfaceIndex ) {
		return a->mSurfaceIndex < b->mSurfaceIndex;
	}
	return a->mPolyIndex < b->mPolyIndex;
}

//
// fixture: a Ghoul2 instance, driven as the game and cgame drive it
//

class Ghoul2Reference : public ::testing::Test {
protected:
	g2handle_t	handle;
	g2t::Bolts	bolts;

	void SetUp() {
		g2t::Init();
		g2t::AddSyntheticModels();
		handle = 0;
	}

	void TearDown() {
		if ( handle ) {
			G2API_CleanGhoul2Models( &handle );
		}
		g2t::Init();
	}

	CGhoul2Info_v &G2( void ) {
		return *G2API_GetGhoul2Model( handle );
	}

	// the skeleton build the game and cgame trigger through G2API_GetBoltMatrix
	void BuildThroughBolt( int time ) {
		mdxaBone_t m;
		const vec3_t origin = { 128.0f, -64.0f, 24.0f };
		ASSERT_TRUE( G2API_GetBoltMatrix( handle, 0, bolts.rHand, &m, kNoAngles, origin, time, NULL, kNoScale ) );
		EXPECT_EQ( time, G2()[0].mSkelFrameNum );
	}

	void BuildSkeleton( int time ) {
		G2_ConstructGhoulSkeleton( G2(), time, NULL, true, kNoAngles, vec3_origin, kNoScale, false );
	}

	// One client frame: R_AddGhoulSurfaces, then RB_SurfaceGhoul for each
	// draw surface, twice (the dynamic glow pass draws them again), checked
	// against the skinning kernel it uses
	void RenderFrame( int time, g2t::Digest &draw, g2t::Digest &scalar, g2t::Digest &sse2,
					  qhandle_t customShader = 0, qhandle_t customSkin = 0, int renderfx = 0 ) {
		trRefEntity_t ent;
		memset( &ent, 0, sizeof( ent ) );
		ent.e.ghoul2 = handle;
		ent.e.radius = 64.0f;
		ent.e.origin[0] = 256.0f;
		ent.e.customShader = customShader;
		ent.e.customSkin = customSkin;
		ent.e.renderfx = renderfx;

		tr.refdef.time = time;
		R_ResetRenderableSurfaces();
		g2t::ClearDrawSurfs();
		R_AddGhoulSurfaces( &ent );
		ASSERT_FALSE( g2t::drawSurfs.empty() );

		CGhoul2Info_v &ghoul2 = G2();
		draw.Int( (int)g2t::drawSurfs.size() );
		for ( int pass = 0; pass < 2; pass++ ) {
			for ( size_t i = 0; i < g2t::drawSurfs.size(); i++ ) {
				const g2t::DrawSurf &ds = g2t::drawSurfs[i];
				const mdxmSurface_t *surface = ds.surf->surfaceData;
				const mdxaBone_v &bones = *(const mdxaBone_v *)ds.surf->boneList;

				// batches start anywhere in tess
				const int base = (int)( ( i * 7 + pass * 3 ) % 32 );
				tess.numVertexes = base;
				tess.numIndexes = base * 3;
				RB_SurfaceGhoul( ds.surf );
				ASSERT_EQ( base + surface->numVerts, tess.numVertexes );
				ASSERT_EQ( base * 3 + surface->numTriangles * 3, tess.numIndexes );

				G2_PLATFORM_SKIN( surface, bones, g2t::skinXyz, g2t::skinNormal );
				EXPECT_TRUE( SameVertexes( &tess.xyz[base], g2t::skinXyz, surface->numVerts ) );
				EXPECT_TRUE( SameVertexes( &tess.normal[base], g2t::skinNormal, surface->numVerts ) );
				if ( pass ) {
					continue;
				}

				draw.Int( ModelOfBoneList( ghoul2, ds.surf->boneList ) );
				draw.Int( surface->thisSurfaceIndex );
				draw.Int( surface->numVerts );
				draw.String( ds.shader ? ds.shader->name : "<none>" );
				draw.Int( ds.fogIndex );
				for ( int k = 0; k < surface->numTriangles * 3; k++ ) {
					draw.Int( (int)tess.indexes[base * 3 + k] - base );
				}
				draw.Floats( &tess.texCoords[0][base][0], 2 * surface->numVerts );

				G2_SkinVertexes( surface, bones, g2t::skinXyz, g2t::skinNormal );
				HashSkinned( scalar, g2t::skinXyz, g2t::skinNormal, surface->numVerts );
#if id386 || idx64
				G2_SkinVertexesSSE2( surface, bones, g2t::skinXyz, g2t::skinNormal );
				HashSkinned( sse2, g2t::skinXyz, g2t::skinNormal, surface->numVerts );
#endif
			}
		}
		EXPECT_EQ( 0, g2t::tessOverflows );
	}

	// G2API_CollisionDetect, as the game traces a shot or a saber: the
	// records in a canonical order (qsort is not stable), then the vertexes
	// of the collision skinning. Returns the number of hits.
	int Collide( int time, const vec3_t start, const vec3_t end, int useLod, float radius, g2t::Digest &d ) {
		CollisionRecord_t records[MAX_G2_COLLISIONS];
		memset( records, 0, sizeof( records ) );
		for ( int i = 0; i < MAX_G2_COLLISIONS; i++ ) {
			records[i].mEntityNum = -1;
			records[i].mDistance = 100000.0f;
		}
		CMiniHeap heap( 2 * 1024 * 1024 );
		const vec3_t origin = { 16.0f, 8.0f, 0.0f };
		G2API_CollisionDetect( records, handle, kNoAngles, origin, time, 5, start, end, kNoScale, &heap, G2_COLLIDE, useLod, radius );

		std::vector<const CollisionRecord_t *> hits;
		for ( int i = 0; i < MAX_G2_COLLISIONS; i++ ) {
			if ( records[i].mEntityNum != -1 ) {
				hits.push_back( &records[i] );
			}
		}
		std::sort( hits.begin(), hits.end(), RecordLess );
		d.Int( (int)hits.size() );
		for ( size_t i = 0; i < hits.size(); i++ ) {
			const CollisionRecord_t &r = *hits[i];
			d.Float( r.mDistance );
			d.Int( r.mEntityNum );
			d.Int( r.mModelIndex );
			d.Int( r.mPolyIndex );
			d.Int( r.mSurfaceIndex );
			d.Floats( r.mCollisionPosition, 3 );
			d.Floats( r.mCollisionNormal, 3 );
			d.Int( r.mFlags );
			d.Int( r.mMaterial );
			d.Int( r.mLocation );
			d.Float( r.mBarycentricI );
			d.Float( r.mBarycentricJ );
		}

		CGhoul2Info_v &ghoul2 = G2();
		for ( size_t i = 0; i < ghoul2.size(); i++ ) {
			model_t *mod = R_GetModelByHandle( RE_RegisterModel( ghoul2[i].mFileName ) );
			const int lod = G2_DecideTraceLod( ghoul2[i], useLod, mod );
			for ( int s = 0; s < mod->mdxm->numSurfaces; s++ ) {
				const float *verts = (const float *)ghoul2[i].mTransformedVertsArray[s];
				d.Int( verts ? 1 : 0 );
				if ( verts ) {
					const mdxmSurface_t *surface = (const mdxmSurface_t *)G2_FindSurface( mod, s, lod );
					d.Floats( verts, surface->numVerts * 5 );
				}
			}
		}
		return (int)hits.size();
	}
};

//
// inputs
//

// The generator must give the same bytes everywhere, or no other value can hold
TEST_F( Ghoul2Reference, SyntheticFilesAreIdenticalEverywhere ) {
	const char *files[] = { g2t::kCharGLA, g2t::kCharGLM, g2t::kPropGLA, g2t::kPropGLM };
	for ( size_t i = 0; i < sizeof( files ) / sizeof( files[0] ); i++ ) {
		g2t::Digest d( std::string( "files/" ) + files[i] );
		HashFile( d, files[i] );
		ExpectGolden( d );
	}
}

TEST_F( Ghoul2Reference, SyntheticModelsLoad ) {
	model_t *glm = g2t::ModelByName( g2t::kCharGLM );
	ASSERT_EQ( MOD_MDXM, glm->type );
	EXPECT_EQ( (int)g2t::kCharLods, glm->numLods );
	EXPECT_EQ( (int)g2t::kCharSurfaces, glm->mdxm->numSurfaces );
	model_t *gla = R_GetModelByHandle( glm->mdxm->animIndex );
	ASSERT_EQ( MOD_MDXA, gla->type );
	EXPECT_EQ( (int)g2t::kCharBones, gla->mdxa->numBones );
	EXPECT_EQ( (int)g2t::kCharFrames, gla->mdxa->numFrames );

	model_t *prop = g2t::ModelByName( g2t::kPropGLM );
	ASSERT_EQ( MOD_MDXM, prop->type );
	EXPECT_EQ( MOD_MDXA, R_GetModelByHandle( prop->mdxm->animIndex )->type );

	EXPECT_EQ( 0, RE_RegisterModel( "models/test/missing.glm" ) );
	EXPECT_EQ( MOD_BAD, R_GetModelByHandle( 0 )->type );
}

//
// kernels
//

TEST_F( Ghoul2Reference, Multiply3x4Matrix ) {
	g2t::Digest d( "kernel/multiply_3x4" );
	uint32_t s = 12345;
	for ( int n = 0; n < 2048; n++ ) {
		mdxaBone_t a, b, out;
		float *fa = &a.matrix[0][0], *fb = &b.matrix[0][0];
		for ( int i = 0; i < 12; i++ ) {
			s = s * 1664525u + 1013904223u;
			fa[i] = (float)( (int)( s >> 16 ) - 32768 ) / 4096.0f;
			s = s * 1664525u + 1013904223u;
			fb[i] = (float)( (int)( s >> 16 ) - 32768 ) / 4096.0f;
		}
		Multiply_3x4Matrix( &out, &a, &b );
		d.Matrix( out );
	}
	ExpectGolden( d );
}

TEST_F( Ghoul2Reference, UncompressBones ) {
	g2t::Digest d( "kernel/uncompress" );
	model_t *gla = R_GetModelByHandle( g2t::ModelByName( g2t::kCharGLM )->mdxm->animIndex );
	for ( int f = 0; f < gla->mdxa->numFrames; f++ ) {
		for ( int b = 0; b < gla->mdxa->numBones; b++ ) {
			mdxaBone_t m;
			UnCompressBone( m.matrix, b, gla->mdxa, f );
			d.Matrix( m );
		}
	}
	ExpectGolden( d );
}

// Every surface of every LOD, skinned by both kernels with an animated skeleton
TEST_F( Ghoul2Reference, SkinningKernels ) {
	ASSERT_TRUE( g2t::SetupCharacter( &handle, kT0, &bolts ) );
	BuildThroughBolt( kT0 + 210 );
	const mdxaBone_v &bones = G2()[0].mTempBoneList;
	model_t *mod = g2t::ModelByName( g2t::kCharGLM );

	g2t::Digest scalar( "kernel/skin/scalar" );
	g2t::Digest sse2( "kernel/skin/sse2" );
	int verts = 0;
	for ( int lod = 0; lod < mod->numLods; lod++ ) {
		for ( int s = 0; s < mod->mdxm->numSurfaces; s++ ) {
			const mdxmSurface_t *surface = (const mdxmSurface_t *)G2_FindSurface( mod, s, lod );
			G2_SkinVertexes( surface, bones, g2t::skinXyz, g2t::skinNormal );
			HashSkinned( scalar, g2t::skinXyz, g2t::skinNormal, surface->numVerts );
#if id386 || idx64
			G2_SkinVertexesSSE2( surface, bones, g2t::skinXyz, g2t::skinNormal );
			HashSkinned( sse2, g2t::skinXyz, g2t::skinNormal, surface->numVerts );
#endif
			verts += surface->numVerts;
		}
	}
	EXPECT_GT( verts, 1000 );
	ExpectGolden( scalar );
#if id386 || idx64
	ExpectGolden( sse2 );
#endif
}

//
// skeletons (server: the bone smoothing cvars do not exist)
//

TEST_F( Ghoul2Reference, RestPose ) {
	ASSERT_TRUE( g2t::CreateCharacter( &handle ) );
	g2t::Digest bones( "rest/bones" );
	BuildSkeleton( kT0 );
	g2t::HashBones( bones, G2() );
	ExpectGolden( bones );
}

// One looping animation, fractional frames, wrapping around its end
TEST_F( Ghoul2Reference, LoopingAnimation ) {
	ASSERT_TRUE( g2t::CreateCharacter( &handle ) );
	ASSERT_TRUE( G2API_SetBoneAnim( handle, 0, "model_root", 3, 40, BONE_ANIM_OVERRIDE_LOOP, 1.3f, kT0, -1, -1 ) );
	g2t::Digest bones( "loop/bones" );
	for ( int k = 0; k < 12; k++ ) {
		BuildSkeleton( kT0 + k * 137 );
		g2t::HashBones( bones, G2() );
	}
	ExpectGolden( bones );
}

// A blend between two animations (BONE_ANIM_BLEND), during and after it,
// with the torso on its own animation
TEST_F( Ghoul2Reference, AnimationBlend ) {
	ASSERT_TRUE( g2t::CreateCharacter( &handle ) );
	ASSERT_TRUE( G2API_SetBoneAnim( handle, 0, "model_root", 2, 20, BONE_ANIM_OVERRIDE_LOOP, 1.0f, kT0, -1, -1 ) );
	ASSERT_TRUE( G2API_SetBoneAnim( handle, 0, "lower_lumbar", 10, 30, BONE_ANIM_OVERRIDE_LOOP | BONE_ANIM_BLEND, 0.8f, kT0, 12.5f, 150 ) );
	ASSERT_TRUE( G2API_SetBoneAnim( handle, 0, "model_root", 25, 45, BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND, 1.0f, kT0 + 400, -1, 300 ) );
	g2t::Digest bones( "blend/bones" );
	static const int times[] = { 400, 475, 550, 625, 699, 701, 800, 1500 };
	for ( size_t k = 0; k < sizeof( times ) / sizeof( times[0] ); k++ ) {
		BuildSkeleton( kT0 + times[k] );
		g2t::HashBones( bones, G2() );
	}
	ExpectGolden( bones );
}

// Negative speeds (looping, frozen), a paused animation, freezes at the
// end, an animation that stops at its end, animations of a single frame
TEST_F( Ghoul2Reference, ReverseFrozenPaused ) {
	ASSERT_TRUE( g2t::CreateCharacter( &handle ) );
	ASSERT_TRUE( G2API_SetBoneAnim( handle, 0, "model_root", 40, 10, BONE_ANIM_OVERRIDE_LOOP, -1.0f, kT0, -1, -1 ) );
	ASSERT_TRUE( G2API_SetBoneAnim( handle, 0, "rhumerus", 30, 20, BONE_ANIM_OVERRIDE_FREEZE, -0.7f, kT0, -1, -1 ) );
	ASSERT_TRUE( G2API_SetBoneAnim( handle, 0, "lhumerus", 5, 25, BONE_ANIM_OVERRIDE_LOOP, 1.0f, kT0, -1, -1 ) );
	ASSERT_TRUE( G2API_SetBoneAnim( handle, 0, "rfemurYZ", 30, 34, BONE_ANIM_OVERRIDE_FREEZE, 2.0f, kT0, -1, -1 ) );
	ASSERT_TRUE( G2API_SetBoneAnim( handle, 0, "lhand", 5, 15, BONE_ANIM_OVERRIDE, 1.0f, kT0, -1, -1 ) );
	ASSERT_TRUE( G2API_SetBoneAnim( handle, 0, "ltibia", 12, 12, BONE_ANIM_OVERRIDE_LOOP, 1.0f, kT0, -1, -1 ) );
	ASSERT_TRUE( G2API_SetBoneAnim( handle, 0, "rtibia", 20, 20, BONE_ANIM_OVERRIDE_LOOP, -1.0f, kT0, -1, -1 ) );
	ASSERT_TRUE( G2API_PauseBoneAnim( &G2()[0], "lhumerus", kT0 + 330 ) );
	g2t::Digest bones( "reverse/bones" );
	// at 1475 model_root is between frames 11 and 10: the reversed loop's virtual frame
	static const int times[] = { 0, 173, 346, 519, 692, 865, 1038, 1211, 1384, 1475, 1557, 2000 };
	for ( size_t k = 0; k < sizeof( times ) / sizeof( times[0] ); k++ ) {
		BuildSkeleton( kT0 + times[k] );
		g2t::HashBones( bones, G2() );
	}
	ExpectGolden( bones );
}

// Bone angle overrides: POSTMULT (torso aim, head look), REPLACE, PREMULT
// on the root and on another bone, and a REPLACE from angles with a blend
// time (its blend branch runs once the blend time is over)
TEST_F( Ghoul2Reference, BoneAngleOverrides ) {
	ASSERT_TRUE( g2t::CreateCharacter( &handle ) );
	ASSERT_TRUE( g2t::AnimateCharacter( handle, kT0 ) );
	ASSERT_TRUE( G2API_SetBoneAnglesMatrix( &G2()[0], "lhumerus", g2t::RotationMatrix( 0.95, -0.2, 0.2, 0.1 ), BONE_ANGLES_REPLACE, NULL, 0, kT0 ) );
	ASSERT_TRUE( G2API_SetBoneAnglesMatrix( &G2()[0], "lclavical", g2t::RotationMatrix( 0.98, 0.1, 0.0, -0.15, 0.0f, 0.5f, 0.0f ), BONE_ANGLES_PREMULT, NULL, 0, kT0 ) );
	ASSERT_TRUE( G2API_SetBoneAngles( handle, 0, "rclavical", kNoAngles, BONE_ANGLES_REPLACE, POSITIVE_Z, NEGATIVE_Y, NEGATIVE_X, NULL, 200, kT0 ) );
	g2t::Digest bones( "angles/bones" );
	static const int times[] = { 0, 50, 199, 201, 260, 640 };
	for ( size_t k = 0; k < sizeof( times ) / sizeof( times[0] ); k++ ) {
		BuildSkeleton( kT0 + times[k] );
		g2t::HashBones( bones, G2() );
	}
	ExpectGolden( bones );
}

// Bolts on tag surfaces, on bones (always transformed, never transformed,
// used by no surface), on a generated surface, and a model bolted to another
TEST_F( Ghoul2Reference, BoltsAndAttachedModel ) {
	ASSERT_TRUE( g2t::SetupCharacter( &handle, kT0, &bolts ) );
	g2t::Digest bones( "bolts/bones" );
	g2t::Digest boltDigest( "bolts/bolts" );
	for ( int k = 0; k < 12; k++ ) {
		BuildThroughBolt( kT0 + k * 50 + ( k * k ) % 7 );
		g2t::HashBones( bones, G2() );
		g2t::HashBolts( boltDigest, G2() );
	}
	ExpectGolden( bones );
	ExpectGolden( boltDigest );
}

// A model whose origin is one of its bolts (G2API_SetNewOrigin)
TEST_F( Ghoul2Reference, NewOrigin ) {
	ASSERT_TRUE( g2t::SetupCharacter( &handle, kT0, &bolts ) );
	ASSERT_TRUE( G2API_SetNewOrigin( handle, bolts.lumbar ) );
	g2t::Digest bones( "neworigin/bones" );
	g2t::Digest boltDigest( "neworigin/bolts" );
	for ( int k = 0; k < 6; k++ ) {
		BuildThroughBolt( kT0 + 30 + k * 90 );
		g2t::HashBones( bones, G2() );
		g2t::HashBolts( boltDigest, G2() );
	}
	ExpectGolden( bones );
	ExpectGolden( boltDigest );

	// R_AddGhoulSurfaces has its own new origin code
	g2t::Digest draw( "neworigin/render/draw" );
	g2t::Digest scalar( "neworigin/render/skin/scalar" );
	g2t::Digest sse2( "neworigin/render/skin/sse2" );
	g2t::Digest renderBones( "neworigin/render/bones" );
	for ( int k = 0; k < 2; k++ ) {
		RenderFrame( kT0 + 700 + k * 45, draw, scalar, sse2 );
		g2t::HashBones( renderBones, G2() );
	}
	ExpectGolden( draw );
	ExpectGolden( scalar );
#if id386 || idx64
	ExpectGolden( sse2 );
#endif
	ExpectGolden( renderBones );
}

// Surfaces turned off and on, then another root surface: the bones no
// surface uses any more keep their stale matrices. No bolt on a generated
// surface here: G2_RemoveRedundantBolts (called by G2_SetRootSurface) reads
// past the end of the bolt list after it removes one (G2_FindOverrideSurface
// never finds a generated surface), and what it reads there is undefined.
TEST_F( Ghoul2Reference, SurfaceOverridesAndRoot ) {
	ASSERT_TRUE( g2t::CreateCharacter( &handle ) );
	ASSERT_TRUE( g2t::AnimateCharacter( handle, kT0 ) );
	ASSERT_TRUE( g2t::AddCharacterBolts( handle, &bolts, false ) );
	ASSERT_TRUE( g2t::AttachProp( handle, kT0, &bolts ) );
	g2t::Digest bones( "surfaces/bones" );
	g2t::Digest boltDigest( "surfaces/bolts" );
	BuildThroughBolt( kT0 + 40 );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "cloak", G2SURFACEFLAG_OFF ) );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "l_leg", G2SURFACEFLAG_OFF | G2SURFACEFLAG_NODESCENDANTS ) );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "head_cap_torso_off", 0 ) );
	BuildThroughBolt( kT0 + 95 );
	g2t::HashBones( bones, G2() );
	g2t::HashBolts( boltDigest, G2() );
	ASSERT_TRUE( G2API_SetRootSurface( handle, 0, "torso" ) );
	BuildThroughBolt( kT0 + 160 );
	g2t::HashBones( bones, G2() );
	g2t::HashBolts( boltDigest, G2() );
	ExpectGolden( bones );
	ExpectGolden( boltDigest );
}

//
// client: bone smoothing and unsquash, the render path
//

// r_ghoul2animsmooth .3 and r_ghoul2unsqashaftersmooth 1 blend each bone
// with the previous skeleton, so every frame depends on the ones before
TEST_F( Ghoul2Reference, ClientSmoothing ) {
	g2t::SetEngineCvars( true );
	ASSERT_TRUE( g2t::SetupCharacter( &handle, kT0, &bolts ) );
	g2t::Digest bones( "smooth/bones" );
	g2t::Digest boltDigest( "smooth/bolts" );
	static const int times[] = { 0, 16, 32, 48, 56, 72, 88, 104, 120, 620, 636, 652 };
	for ( size_t k = 0; k < sizeof( times ) / sizeof( times[0] ); k++ ) {
		tr.refdef.time = kT0 + times[k];
		BuildThroughBolt( kT0 + times[k] );
		g2t::HashBones( bones, G2() );
		g2t::HashBolts( boltDigest, G2() );
	}
	ExpectGolden( bones );
	ExpectGolden( boltDigest );
}

// R_AddGhoulSurfaces then RB_SurfaceGhoul, in each LOD
TEST_F( Ghoul2Reference, RenderLods ) {
	g2t::SetEngineCvars( true );
	ASSERT_TRUE( g2t::SetupCharacter( &handle, kT0, &bolts ) );

	// what ProjectRadius returns for LOD 0, 1 and 2 (r_lodscale 5)
	static const float radius[3] = { 0.0f, 0.12f, 0.04f };
	for ( int lod = 0; lod < 3; lod++ ) {
		char name[64];
		Com_sprintf( name, sizeof( name ), "render/lod%d/", lod );
		g2t::Digest draw( std::string( name ) + "draw" );
		g2t::Digest scalar( std::string( name ) + "skin/scalar" );
		g2t::Digest sse2( std::string( name ) + "skin/sse2" );
		g2t::Digest bones( std::string( name ) + "bones" );
		g2t::Digest boltDigest( std::string( name ) + "bolts" );
		g2t::projectedRadius = radius[lod];
		for ( int k = 0; k < 3; k++ ) {
			RenderFrame( kT0 + 1000 * lod + k * 40, draw, scalar, sse2 );
			EXPECT_EQ( 10u, g2t::drawSurfs.size() );	// 8 surfaces of the character, 2 of the prop
			g2t::HashBones( bones, G2() );
			g2t::HashBolts( boltDigest, G2() );
		}
		ExpectGolden( draw );
		ExpectGolden( scalar );
#if id386 || idx64
		ExpectGolden( sse2 );
#endif
		ExpectGolden( bones );
		ExpectGolden( boltDigest );
	}
}

// Shadows (cg_shadows 2, stencil, and 3, projected) add a second draw
// surface per surface; skins (the instance's, the entity's, the model's) and
// a custom shader change the shaders
TEST_F( Ghoul2Reference, RenderShadowsSkinsShader ) {
	g2t::SetEngineCvars( true );
	ASSERT_TRUE( g2t::SetupCharacter( &handle, kT0, &bolts ) );
	g2t::Digest draw( "render/variants/draw" );
	g2t::Digest scalar( "render/variants/skin/scalar" );
	g2t::Digest sse2( "render/variants/skin/sse2" );

	g2t::shadows.integer = 2;
	RenderFrame( kT0 + 10, draw, scalar, sse2 );
	EXPECT_EQ( 20u, g2t::drawSurfs.size() );
	g2t::shadows.integer = 3;
	RenderFrame( kT0 + 30, draw, scalar, sse2, 0, 0, RF_SHADOW_PLANE );
	EXPECT_EQ( 20u, g2t::drawSurfs.size() );
	g2t::shadows.integer = 1;

	std::vector<std::string> red, blue;
	red.push_back( "torso" );
	red.push_back( "skins/red_torso" );
	red.push_back( "head" );
	red.push_back( "skins/red_head" );
	blue.push_back( "blade" );
	blue.push_back( "skins/blue_blade" );
	const qhandle_t redSkin = g2t::RegisterSkin( "skins/red", red );
	const qhandle_t blueSkin = g2t::RegisterSkin( "skins/blue", blue );

	G2()[0].mCustomSkin = redSkin;		// model 0; the entity's skin for model 1
	RenderFrame( kT0 + 50, draw, scalar, sse2, 0, blueSkin );
	G2()[0].mCustomSkin = 0;
	G2()[0].mSkin = redSkin;
	RenderFrame( kT0 + 70, draw, scalar, sse2 );
	RenderFrame( kT0 + 90, draw, scalar, sse2, g2t::ShaderHandle( "custom/glow" ) );

	ExpectGolden( draw );
	ExpectGolden( scalar );
#if id386 || idx64
	ExpectGolden( sse2 );
#endif
}

//
// collision (server): the collision skinning and the ray tests
//

TEST_F( Ghoul2Reference, Collision ) {
	ASSERT_TRUE( g2t::SetupCharacter( &handle, kT0, &bolts ) );
	g2t::Digest d( "collision" );
	const vec3_t torsoStart = { 80.0f, 8.0f, 50.0f }, torsoEnd = { -60.0f, 8.0f, 50.0f };
	const vec3_t armStart = { 18.0f, 60.0f, 46.0f }, armEnd = { 18.0f, -60.0f, 40.0f };
	for ( int useLod = 0; useLod < 2; useLod++ ) {
		EXPECT_GT( Collide( kT0 + 60, torsoStart, torsoEnd, useLod, 0.0f, d ), 0 );
		EXPECT_GT( Collide( kT0 + 333, armStart, armEnd, useLod, 0.0f, d ), 0 );
		EXPECT_GT( Collide( kT0 + 333, torsoStart, torsoEnd, useLod, 6.0f, d ), 0 );
	}
	ExpectGolden( d );
}

//
// Kyle, from the retail assets0.pk3 when there is one (read only, never
// copied): the same checks on a real player model. Its reference values
// hold for the original JK2 files, which the first check recognizes; other
// files skip the test.
//

class Ghoul2Retail : public Ghoul2Reference {
protected:
	void SetUp() {
		Ghoul2Reference::SetUp();
		std::string reason;
		if ( !Load( &reason ) ) {
			GTEST_SKIP() << reason;
		}
	}

	static bool Load( std::string *reason ) {
		static int state = 0;	// 0 not tried, 1 loaded, -1 unavailable
		static std::string why;
		if ( state == 0 ) {
			state = -1;
			const std::string base = g2t::FindRetailBase( g2t::ProgramPath().c_str() );
			std::vector<std::string> names;
			names.push_back( g2t::kKyleGLM );
			names.push_back( g2t::kHumanoidGLA );
			if ( base.empty() ) {
				why = "no retail assets0.pk3 (set JK2MV_TEST_BASE to the folder that holds it)";
			} else if ( !g2t::LoadFromPk3( base + "/assets0.pk3", names ) ) {
				why = "the Kyle model is not in " + base + "/assets0.pk3";
			} else if ( !IsReferenceFile( g2t::kKyleGLM ) || !IsReferenceFile( g2t::kHumanoidGLA ) ) {
				why = "not the original JK2 files, the reference values do not apply";
			} else {
				state = 1;
			}
		}
		*reason = why;
		return state == 1;
	}

	static bool IsReferenceFile( const char *name ) {
		g2t::Digest d( std::string( "retail/files/" ) + name );
		HashFile( d, name );
		if ( PrintGoldens() ) {
			printf( "\t{ \"%s\", 0x%016llxULL },\n", d.Name().c_str(), (unsigned long long)d.Value() );
		}
		const Golden *g = FindGolden( d.Name() );
		return !g || g->value == d.Value();	// no value yet: recording
	}
};

TEST_F( Ghoul2Retail, KyleSkeletonAndBolts ) {
	ASSERT_TRUE( g2t::SetupKyle( &handle, kT0, &bolts ) );
	g2t::Digest bones( "retail/bones" );
	g2t::Digest boltDigest( "retail/bolts" );
	for ( int k = 0; k < 8; k++ ) {
		BuildThroughBolt( kT0 + k * 61 );
		g2t::HashBones( bones, G2() );
		g2t::HashBolts( boltDigest, G2() );
	}
	ExpectGolden( bones );
	ExpectGolden( boltDigest );
}

TEST_F( Ghoul2Retail, KyleRenderLods ) {
	g2t::SetEngineCvars( true );
	ASSERT_TRUE( g2t::SetupKyle( &handle, kT0, &bolts ) );
	ASSERT_EQ( 4, g2t::ModelByName( g2t::kKyleGLM )->numLods );

	// ProjectRadius values that pick LOD 0 to 3 (r_lodscale 5, 4 LODs)
	static const float radius[4] = { 0.0f, 0.12f, 0.06f, 0.02f };
	for ( int lod = 0; lod < 4; lod++ ) {
		char name[64];
		Com_sprintf( name, sizeof( name ), "retail/render/lod%d/", lod );
		g2t::Digest draw( std::string( name ) + "draw" );
		g2t::Digest scalar( std::string( name ) + "skin/scalar" );
		g2t::Digest sse2( std::string( name ) + "skin/sse2" );
		g2t::Digest bones( std::string( name ) + "bones" );
		g2t::Digest boltDigest( std::string( name ) + "bolts" );
		g2t::projectedRadius = radius[lod];
		for ( int k = 0; k < 3; k++ ) {
			RenderFrame( kT0 + 1000 * lod + k * 33, draw, scalar, sse2 );
			g2t::HashBones( bones, G2() );
			g2t::HashBolts( boltDigest, G2() );
		}
		ExpectGolden( draw );
		ExpectGolden( scalar );
#if id386 || idx64
		ExpectGolden( sse2 );
#endif
		ExpectGolden( bones );
		ExpectGolden( boltDigest );
	}
}

TEST_F( Ghoul2Retail, KyleCollision ) {
	ASSERT_TRUE( g2t::SetupKyle( &handle, kT0, &bolts ) );
	g2t::Digest d( "retail/collision" );
	// the model origin is at the waist
	const vec3_t chestStart = { 80.0f, 8.0f, 20.0f }, chestEnd = { -60.0f, 8.0f, 20.0f };
	const vec3_t hipsStart = { 19.0f, 60.0f, 2.0f }, hipsEnd = { 19.0f, -60.0f, 2.0f };
	for ( int useLod = 0; useLod < 2; useLod++ ) {
		EXPECT_GT( Collide( kT0 + 75, chestStart, chestEnd, useLod, 0.0f, d ), 0 );
		EXPECT_GT( Collide( kT0 + 75, hipsStart, hipsEnd, useLod, 0.0f, d ), 0 );
		EXPECT_GT( Collide( kT0 + 75, chestStart, chestEnd, useLod, 6.0f, d ), 0 );
	}
	ExpectGolden( d );
}

} // namespace

int main( int argc, char **argv ) {
	g2t::SetProgramPath( argc > 0 ? argv[0] : "" );
	::testing::InitGoogleTest( &argc, argv );
	return RUN_ALL_TESTS();
}
