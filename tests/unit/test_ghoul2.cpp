// test_ghoul2.cpp - FE-16: bit-exact reference tests of the Ghoul2 code
//
// What runs is the engine's own code (tr_ghoul2.cpp, ghoul2/*.cpp, matcomp.c,
// q_math.cpp), compiled with the engine's definitions, through the API the
// game and cgame modules call, on synthetic GLA/GLM models (ghoul2_synth.cpp)
// and, when the retail assets0.pk3 is found, on Kyle. Each scenario hashes
// what it produced (bone matrices and their smoothing time stamps, bolt
// matrices, the world-space bolt positions G2API_GetBoltMatrix returns, the
// draw surfaces of the frame, skinned positions and normals, collision records
// and vertexes) and compares the hash with a reference value recorded from the
// code before any optimization: one bit off fails.
//
// For the Ghoul2 optimizations (FE-3 caches, FE-4 SIMD, FE-5 skinning once
// per frame):
//  - every reference value must hold, on x86-64 and, for portable code, on
//    ARM64 too (it checks the scalar skinning values);
//  - ClientStaleBones guards the stale matrices of bones no surface uses any
//    more, which the client smoothing and unsquash keep changing (the dead
//    `if (!boneUsedList)` guard of the unsquash loops must stay dead), and
//    TwoInstancesOneFrame two instances of one model that a cache keyed on
//    the model alone would mix up;
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
	{ "files/models/test/g2char.glm", 0xd93524f0502a7eaaULL },
	{ "files/models/test/g2prop.gla", 0x6f1afabeffa018e8ULL },
	{ "files/models/test/g2prop.glm", 0x431cbbfd42db9395ULL },

	// kernels
	{ "kernel/multiply_3x4", 0xceb841ad22f894c9ULL },
	{ "kernel/uncompress", 0xf77fbcbcdd1ccacaULL },
	{ "kernel/skin/scalar", 0x341ac9d7e7807a04ULL },
	{ "kernel/skin/sse2", 0xc3b5e7fdcf74c610ULL },

	// skeletons on the server (no bone smoothing), bolts, attached model, surfaces
	{ "rest/bones", 0x834e4832bd118bafULL },
	{ "loop/bones", 0xbb7d3c77eac3fb6dULL },
	{ "blend/bones", 0x68c857f3185b21beULL },
	{ "reverse/bones", 0x244c0370d1c68ab1ULL },
	{ "angles/bones", 0x862b9772241f4bfcULL },
	{ "bolts/bones", 0x5d375cf1cd705edcULL },
	{ "bolts/bolts", 0x2fe89d6b0553bf5cULL },
	{ "bolts/world", 0x275ff121469f6e9fULL },
	{ "neworigin/bones", 0xb5ae1d0890ed160aULL },
	{ "neworigin/bolts", 0xa20876fd2ab9d4abULL },
	{ "neworigin/world", 0x9cb648e5d4bbd3c5ULL },
	{ "neworigin/render/draw", 0x7a645b53d6532329ULL },
	{ "neworigin/render/skin/scalar", 0xd61627ac047fad50ULL },
	{ "neworigin/render/skin/sse2", 0x64b3fcd8eb332e2cULL },
	{ "neworigin/render/bones", 0x1624939e299b5eefULL },
	{ "surfaces/bones", 0x2ba6fad17da0ab21ULL },
	{ "surfaces/bolts", 0xdb049a7b1127b835ULL },
	{ "surfaces/world", 0x257d5cfe452f83aaULL },

	// client: bone smoothing and unsquash (stale bones included), the render
	// path, two instances in one frame
	{ "smooth/bones", 0x62ff5f4cc8d09cdaULL },
	{ "smooth/bolts", 0xd601d07a139e8e85ULL },
	{ "smooth/world", 0xffcebb746cade28aULL },
	{ "stale/bones", 0x797b365fed3ff421ULL },
	{ "stale/bolts", 0x9f20ab02fb2e4423ULL },
	{ "stale/world", 0xf5ee6b1885cd467fULL },
	{ "stale/render/draw", 0x61e05d95bdfb82edULL },
	{ "stale/render/skin/scalar", 0x1684b2f2b9280104ULL },
	{ "stale/render/skin/sse2", 0x16ceedd67fb21878ULL },
	{ "render/lod0/draw", 0xd506880ebf11a810ULL },
	{ "render/lod0/skin/scalar", 0x4186f935201c0b03ULL },
	{ "render/lod0/skin/sse2", 0x237fcf3e36d68f5bULL },
	{ "render/lod0/bones", 0x1470c532cad8c4c3ULL },
	{ "render/lod0/bolts", 0x3dfa04bbb3a0145fULL },
	{ "render/lod1/draw", 0x6db2fa9e01a5485cULL },
	{ "render/lod1/skin/scalar", 0x85b9567e37174818ULL },
	{ "render/lod1/skin/sse2", 0xe2f3fa17b2e51022ULL },
	{ "render/lod1/bones", 0x1d7650f95877181dULL },
	{ "render/lod1/bolts", 0x180d228fca22ba89ULL },
	{ "render/lod2/draw", 0xf33038569af9f8c4ULL },
	{ "render/lod2/skin/scalar", 0x84c67c4c9f76192cULL },
	{ "render/lod2/skin/sse2", 0x47fa69de0ecd05ebULL },
	{ "render/lod2/bones", 0x10fc6bd56c40067dULL },
	{ "render/lod2/bolts", 0x9877e01a35471c34ULL },
	{ "render/variants/draw", 0xaf8acd1dfe71c64dULL },
	{ "render/variants/skin/scalar", 0x57fc183821d95989ULL },
	{ "render/variants/skin/sse2", 0x18e693d9d01e6a6dULL },
	{ "instances/bones", 0x8131f7f48830b785ULL },
	{ "instances/bolts", 0x9fcfa0c342b5f467ULL },
	{ "instances/world", 0xe683f8827f3f101cULL },
	{ "instances/render/draw", 0x059e02f070dc85e6ULL },
	{ "instances/render/skin/scalar", 0xb0d4edef22203fd4ULL },
	{ "instances/render/skin/sse2", 0x161bd1fc37078937ULL },

	// collision
	{ "collision", 0x24adb185bddd584fULL },

	// Kyle from the retail assets0.pk3 (checked when its files match)
	{ "retail/files/models/players/kyle/model.glm", 0x3a2a9e659aab99d7ULL },
	{ "retail/files/models/players/_humanoid/_humanoid.gla", 0xcb967a89b10ab841ULL },
	{ "retail/bones", 0x3574c7b137ce3869ULL },
	{ "retail/bolts", 0x07ae84c8f4a4a7ddULL },
	{ "retail/world", 0x26e373dface18143ULL },
	{ "retail/render/lod0/draw", 0x971aa6745dedd5acULL },
	{ "retail/render/lod0/skin/scalar", 0xe8259014737824b3ULL },
	{ "retail/render/lod0/skin/sse2", 0xf14101cbd220b449ULL },
	{ "retail/render/lod0/bones", 0x844301a3bfb94d44ULL },
	{ "retail/render/lod0/bolts", 0x6f6d9c558dbf3c11ULL },
	{ "retail/render/lod1/draw", 0xf8dab51a6e4b2589ULL },
	{ "retail/render/lod1/skin/scalar", 0xe4ebf906146b1724ULL },
	{ "retail/render/lod1/skin/sse2", 0x7b3cda51c2b4033eULL },
	{ "retail/render/lod1/bones", 0xac3dfb05ffcc5d3cULL },
	{ "retail/render/lod1/bolts", 0xee51eaa26d0e2c1eULL },
	{ "retail/render/lod2/draw", 0xfa56455739d44ff8ULL },
	{ "retail/render/lod2/skin/scalar", 0x3e05a769a3469a92ULL },
	{ "retail/render/lod2/skin/sse2", 0x2dbdf116c9ecd474ULL },
	{ "retail/render/lod2/bones", 0x5c9c271ab6c314c6ULL },
	{ "retail/render/lod2/bolts", 0x7e0b2e35b6820755ULL },
	{ "retail/render/lod3/draw", 0x9cf2fcb3b7a12820ULL },
	{ "retail/render/lod3/skin/scalar", 0x45540c68c0a5d21aULL },
	{ "retail/render/lod3/skin/sse2", 0x31c2b8b42409ae0eULL },
	{ "retail/render/lod3/bones", 0x5af8cbc39f4e5a1cULL },
	{ "retail/render/lod3/bolts", 0xa5b3be121091f23eULL },
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
const vec3_t kWorldOrigin = { 128.0f, -64.0f, 24.0f };
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

// What G2API_GetBoltMatrix gives the game and cgame (saber and muzzle
// positions), for every bolt in use of every model of the instance, without
// and with a model scale. Only the translation is hashed: the rotation goes
// through a 270 degree turn (Create_Matrix) whose sine and cosine depend on
// the C library, while the translation only meets the world matrix of angles
// 0, whose sines and cosines are exact.
void HashWorldBolts( g2t::Digest &d, g2handle_t h, int time ) {
	static const vec3_t scale = { 1.25f, 0.75f, 1.5f };
	CGhoul2Info_v &ghoul2 = *G2API_GetGhoul2Model( h );
	for ( size_t i = 0; i < ghoul2.size(); i++ ) {
		d.Label( "model" );
		d.Int( (int)i );
		for ( size_t b = 0; b < ghoul2[i].mBltlist.size(); b++ ) {
			if ( ghoul2[i].mBltlist[b].boneNumber == -1 && ghoul2[i].mBltlist[b].surfaceNumber == -1 ) {
				continue;
			}
			d.Int( (int)b );
			mdxaBone_t m;
			ASSERT_TRUE( G2API_GetBoltMatrix( h, (int)i, (int)b, &m, kNoAngles, kWorldOrigin, time, NULL, kNoScale ) );
			d.Float( m.matrix[0][3] );
			d.Float( m.matrix[1][3] );
			d.Float( m.matrix[2][3] );
			ASSERT_TRUE( G2API_GetBoltMatrix( h, (int)i, (int)b, &m, kNoAngles, kWorldOrigin, time, NULL, scale ) );
			d.Float( m.matrix[0][3] );
			d.Float( m.matrix[1][3] );
			d.Float( m.matrix[2][3] );
		}
	}
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
	g2handle_t	handle2;	// a second instance, for the scenes that draw two
	g2t::Bolts	bolts2;

	void SetUp() {
		g2t::Init();
		g2t::AddSyntheticModels();
		handle = 0;
		handle2 = 0;
	}

	void TearDown() {
		if ( handle2 ) {
			G2API_CleanGhoul2Models( &handle2 );
		}
		if ( handle ) {
			G2API_CleanGhoul2Models( &handle );
		}
		g2t::Init();
	}

	CGhoul2Info_v &G2( void ) {
		return *G2API_GetGhoul2Model( handle );
	}

	// The skeleton build the game and cgame trigger through
	// G2API_GetBoltMatrix; with a digest, then the world-space bolts they get
	// back (HashWorldBolts)
	void BuildThroughBolt( int time, g2t::Digest *world = NULL ) {
		BuildInstance( handle, bolts.rHand, time, world );
	}

	void BuildInstance( g2handle_t h, int bolt, int time, g2t::Digest *world ) {
		mdxaBone_t m;
		ASSERT_TRUE( G2API_GetBoltMatrix( h, 0, bolt, &m, kNoAngles, kWorldOrigin, time, NULL, kNoScale ) );
		EXPECT_EQ( time, ( *G2API_GetGhoul2Model( h ) )[0].mSkelFrameNum );
		if ( world ) {
			HashWorldBolts( *world, h, time );
		}
	}

	void BuildSkeleton( int time ) {
		G2_ConstructGhoulSkeleton( G2(), time, NULL, true, kNoAngles, vec3_origin, kNoScale, false );
	}

	// One client frame: R_AddGhoulSurfaces, then RB_SurfaceGhoul for each
	// draw surface, twice (the dynamic glow pass draws them again), checked
	// against the skinning kernel it uses
	void RenderFrame( int time, g2t::Digest &draw, g2t::Digest &scalar, g2t::Digest &sse2,
					  qhandle_t customShader = 0, qhandle_t customSkin = 0, int renderfx = 0 ) {
		RenderEntities( &handle, 1, time, draw, scalar, sse2, customShader, customSkin, renderfx );
	}

	// The same with one entity per instance, added in this order
	void RenderEntities( const g2handle_t *handles, int numEntities, int time, g2t::Digest &draw, g2t::Digest &scalar, g2t::Digest &sse2,
						 qhandle_t customShader = 0, qhandle_t customSkin = 0, int renderfx = 0 ) {
		tr.refdef.time = time;
		R_ResetRenderableSurfaces();
		g2t::ClearDrawSurfs();
		for ( int e = 0; e < numEntities; e++ ) {
			trRefEntity_t ent;
			memset( &ent, 0, sizeof( ent ) );
			ent.e.ghoul2 = handles[e];
			ent.e.radius = 64.0f;
			ent.e.origin[0] = 256.0f + 128.0f * e;
			ent.e.customShader = customShader;
			ent.e.customSkin = customSkin;
			ent.e.renderfx = renderfx;
			R_AddGhoulSurfaces( &ent );
		}
		ASSERT_FALSE( g2t::drawSurfs.empty() );

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

				// the entity (when there are several) and the model whose
				// skeleton the surface uses
				int entity = 0, model = -1;
				for ( ; entity < numEntities; entity++ ) {
					model = ModelOfBoneList( *G2API_GetGhoul2Model( handles[entity] ), ds.surf->boneList );
					if ( model >= 0 ) {
						break;
					}
				}
				if ( numEntities > 1 ) {
					draw.Int( entity );
				}
				draw.Int( model );
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

// Bolts on tag surfaces (one with a single bone, one blending three), on
// bones (always transformed, never transformed, used by no surface), on a
// generated surface, and a model bolted to another; in model space and in
// world space
TEST_F( Ghoul2Reference, BoltsAndAttachedModel ) {
	ASSERT_TRUE( g2t::SetupCharacter( &handle, kT0, &bolts ) );
	g2t::Digest bones( "bolts/bones" );
	g2t::Digest boltDigest( "bolts/bolts" );
	g2t::Digest world( "bolts/world" );
	for ( int k = 0; k < 12; k++ ) {
		BuildThroughBolt( kT0 + k * 50 + ( k * k ) % 7, &world );
		g2t::HashBones( bones, G2() );
		g2t::HashBolts( boltDigest, G2() );
	}
	ExpectGolden( bones );
	ExpectGolden( boltDigest );
	ExpectGolden( world );
}

// A model whose origin is one of its bolts (G2API_SetNewOrigin)
TEST_F( Ghoul2Reference, NewOrigin ) {
	ASSERT_TRUE( g2t::SetupCharacter( &handle, kT0, &bolts ) );
	ASSERT_TRUE( G2API_SetNewOrigin( handle, bolts.lumbar ) );
	g2t::Digest bones( "neworigin/bones" );
	g2t::Digest boltDigest( "neworigin/bolts" );
	g2t::Digest world( "neworigin/world" );
	for ( int k = 0; k < 6; k++ ) {
		BuildThroughBolt( kT0 + 30 + k * 90, &world );
		g2t::HashBones( bones, G2() );
		g2t::HashBolts( boltDigest, G2() );
	}
	ExpectGolden( bones );
	ExpectGolden( boltDigest );
	ExpectGolden( world );

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
	g2t::Digest world( "surfaces/world" );
	BuildThroughBolt( kT0 + 40 );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "cloak", G2SURFACEFLAG_OFF ) );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "l_leg", G2SURFACEFLAG_OFF | G2SURFACEFLAG_NODESCENDANTS ) );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "head_cap_torso_off", 0 ) );
	BuildThroughBolt( kT0 + 95, &world );
	g2t::HashBones( bones, G2() );
	g2t::HashBolts( boltDigest, G2() );
	ASSERT_TRUE( G2API_SetRootSurface( handle, 0, "torso" ) );
	BuildThroughBolt( kT0 + 160, &world );
	g2t::HashBones( bones, G2() );
	g2t::HashBolts( boltDigest, G2() );
	ExpectGolden( bones );
	ExpectGolden( boltDigest );
	ExpectGolden( world );
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
	g2t::Digest world( "smooth/world" );
	static const int times[] = { 0, 16, 32, 48, 56, 72, 88, 104, 120, 620, 636, 652 };
	for ( size_t k = 0; k < sizeof( times ) / sizeof( times[0] ); k++ ) {
		tr.refdef.time = kT0 + times[k];
		BuildThroughBolt( kT0 + times[k], &world );
		g2t::HashBones( bones, G2() );
		g2t::HashBolts( boltDigest, G2() );
	}
	ExpectGolden( bones );
	ExpectGolden( boltDigest );
	ExpectGolden( world );
}

// Client smoothing and unsquash on bones that stop being used, then are used
// again: surfaces turned off, another root surface, the whole body back. A
// bone no surface uses keeps the matrix of the last frame it was built, but
// both unsquash loops still go over it (their `if (!boneUsedList)` guard is
// dead), so each frame changes that stale matrix a little; the bolts on the
// bone follow it, and once the bone is used again the smoothing blends its new
// pose with it. The FE-3 caches must keep this as it is (ROADMAP.md: do not
// "fix" the guard). Even frames build the skeleton through a bolt
// (G2_ConstructGhoulSkeleton) and draw it, odd frames build it in
// R_AddGhoulSurfaces: each of the two unsquash loops runs on stale bones.
TEST_F( Ghoul2Reference, ClientStaleBones ) {
	g2t::SetEngineCvars( true );
	ASSERT_TRUE( g2t::CreateCharacter( &handle ) );
	ASSERT_TRUE( g2t::AnimateCharacter( handle, kT0 ) );
	// no bolt on a generated surface: see SurfaceOverridesAndRoot
	ASSERT_TRUE( g2t::AddCharacterBolts( handle, &bolts, false ) );
	ASSERT_TRUE( g2t::AttachProp( handle, kT0, &bolts ) );
	ASSERT_GE( G2API_AddBolt( handle, 0, "ltibia" ), 0 );
	ASSERT_GE( G2API_AddBolt( handle, 0, "rtibia" ), 0 );

	g2t::Digest bones( "stale/bones" );
	g2t::Digest boltDigest( "stale/bolts" );
	g2t::Digest world( "stale/world" );
	g2t::Digest draw( "stale/render/draw" );
	g2t::Digest scalar( "stale/render/skin/scalar" );
	g2t::Digest sse2( "stale/render/skin/sse2" );
	for ( int k = 0; k < 24; k++ ) {
		if ( k == 3 ) {
			// no surface uses ltibia and ltalus any more
			ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "cloak", G2SURFACEFLAG_OFF ) );
			ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "l_leg", G2SURFACEFLAG_OFF | G2SURFACEFLAG_NODESCENDANTS ) );
		} else if ( k == 9 ) {
			// below the torso, no surface uses a leg bone. G2_SetRootSurface
			// also drops the overrides and the bolts it finds outside the new
			// root (the cloak comes back, the bolts on the shins go): turn the
			// cloak off and bolt the shins again
			ASSERT_TRUE( G2API_SetRootSurface( handle, 0, "torso" ) );
			ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "cloak", G2SURFACEFLAG_OFF ) );
			ASSERT_GE( G2API_AddBolt( handle, 0, "rtibia" ), 0 );
			ASSERT_GE( G2API_AddBolt( handle, 0, "ltibia" ), 0 );
		} else if ( k == 15 ) {
			// the whole body again (this drops the cloak override too): the
			// leg bones are smoothed from their stale matrices
			ASSERT_TRUE( G2API_SetRootSurface( handle, 0, "hips" ) );
		}
		const int t = kT0 + 16 * k;
		if ( !( k & 1 ) ) {
			tr.refdef.time = t;
			BuildThroughBolt( t, &world );
		}
		RenderFrame( t, draw, scalar, sse2 );
		g2t::HashBones( bones, G2() );
		g2t::HashBolts( boltDigest, G2() );
	}
	ExpectGolden( bones );
	ExpectGolden( boltDigest );
	ExpectGolden( world );
	ExpectGolden( draw );
	ExpectGolden( scalar );
#if id386 || idx64
	ExpectGolden( sse2 );
#endif
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

// Two instances of the same model in one frame, one drawn after the other,
// that differ in surfaces (cloak and left leg off, the head replaced by its
// cap), skin, animation and, every other frame, LOD: what FE-3 caches per
// model and state (used bones, skin shaders) and FE-5 per surface must keep
// them apart, whichever comes first, in the same LOD or not
TEST_F( Ghoul2Reference, TwoInstancesOneFrame ) {
	g2t::SetEngineCvars( true );
	ASSERT_TRUE( g2t::SetupCharacter( &handle, kT0, &bolts ) );
	ASSERT_TRUE( g2t::CreateCharacter( &handle2 ) );
	ASSERT_TRUE( g2t::AnimateCharacter( handle2, kT0 - 270 ) );
	ASSERT_TRUE( g2t::AddCharacterBolts( handle2, &bolts2, false ) );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle2, "cloak", G2SURFACEFLAG_OFF ) );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle2, "l_leg", G2SURFACEFLAG_OFF | G2SURFACEFLAG_NODESCENDANTS ) );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle2, "head", G2SURFACEFLAG_OFF ) );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle2, "head_cap_torso_off", 0 ) );

	// a skin each, as every player has
	std::vector<std::string> blue, red;
	blue.push_back( "torso" );
	blue.push_back( "skins/blue_torso" );
	blue.push_back( "head" );
	blue.push_back( "skins/blue_head" );
	red.push_back( "torso" );
	red.push_back( "skins/red_torso" );
	red.push_back( "head_cap_torso_off" );
	red.push_back( "skins/red_cap" );
	CGhoul2Info_v &second = *G2API_GetGhoul2Model( handle2 );
	ASSERT_TRUE( G2API_SetSkin( &G2()[0], g2t::RegisterSkin( "skins/blue", blue ) ) );
	ASSERT_TRUE( G2API_SetSkin( &second[0], g2t::RegisterSkin( "skins/red", red ) ) );

	g2t::Digest bones( "instances/bones" );
	g2t::Digest boltDigest( "instances/bolts" );
	g2t::Digest world( "instances/world" );
	g2t::Digest draw( "instances/render/draw" );
	g2t::Digest scalar( "instances/render/skin/scalar" );
	g2t::Digest sse2( "instances/render/skin/sse2" );
	for ( int k = 0; k < 6; k++ ) {
		const int t = kT0 + 40 + 16 * k;
		if ( k % 3 != 2 ) {
			// the cgame asks both for a bolt first; on every third frame the
			// renderer builds both skeletons itself
			tr.refdef.time = t;
			BuildInstance( handle, bolts.rHand, t, &world );
			BuildInstance( handle2, bolts2.lHand, t, &world );
		}
		// the second in LOD 0 like the first (the same mesh surfaces) on
		// even frames, in LOD 1 on odd ones, and then drawn first
		ASSERT_TRUE( G2API_SetLodBias( &second[0], k & 1 ) );
		const g2handle_t order[2] = { ( k & 1 ) ? handle2 : handle, ( k & 1 ) ? handle : handle2 };
		RenderEntities( order, 2, t, draw, scalar, sse2 );
		// the first: 8 surfaces and the prop's 2; the second: 6 surfaces
		EXPECT_EQ( 16u, g2t::drawSurfs.size() );
		g2t::HashBones( bones, G2() );
		g2t::HashBones( bones, second );
		g2t::HashBolts( boltDigest, G2() );
		g2t::HashBolts( boltDigest, second );
	}
	ExpectGolden( bones );
	ExpectGolden( boltDigest );
	ExpectGolden( world );
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
	g2t::Digest world( "retail/world" );
	for ( int k = 0; k < 8; k++ ) {
		BuildThroughBolt( kT0 + k * 61, &world );
		g2t::HashBones( bones, G2() );
		g2t::HashBolts( boltDigest, G2() );
	}
	ExpectGolden( bones );
	ExpectGolden( boltDigest );
	ExpectGolden( world );
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
