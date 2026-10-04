// bench_ghoul2.cpp - FE-16: micro-benchmarks of the Ghoul2 hot paths
//
// Times the engine's own Ghoul2 code (see ghoul2_harness.h) on the synthetic
// character with its prop, and on Kyle when the retail assets0.pk3 is found
// ($JK2MV_TEST_BASE, or base/ next to the executable):
//
//   skeleton    G2_ConstructGhoulSkeleton as G2API_GetBoltMatrix and
//               R_AddGhoulSurfaces run it (client cvars: bone smoothing and
//               unsquash), per call and per bone of the instance
//   bones       G2_TransformGhoulBones alone, every bone of model 0 used
//   skin        the skinning kernels on the surfaces drawn in LOD 0
//   surface     RB_SurfaceGhoul on the same surfaces (indexes, skinning,
//               texture coordinates)
//   frontend    R_AddGhoulSurfaces: skeleton, surface walk, bolts
//   collision   G2_TransformModel, the collision skinning of the server
//   multiply    Multiply_3x4Matrix
//   uncompress  UnCompressBone (MC_UnCompressQuat)
//
// It prints the best of several runs in nanoseconds and never fails on a
// time: compare its output before and after a Ghoul2 change (FE-3, FE-4,
// FE-5), same build type, on an idle machine. --quick runs each benchmark
// briefly, on the synthetic models only (the ctest smoke run).

#include "ghoul2_harness.h"

#include <chrono>
#include <stdio.h>
#include <string.h>

namespace {

bool		quick;
unsigned	sink;	// keeps results alive

unsigned Bits( float f ) {
	unsigned u;
	memcpy( &u, &f, sizeof( u ) );
	return u;
}

template <class F>
double TimeOnce( F &f, int iterations ) {
	const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	f( iterations );
	return std::chrono::duration<double, std::nano>( std::chrono::steady_clock::now() - start ).count();
}

// Nanoseconds per iteration: the iteration count doubles until a run takes
// 20 ms (0.2 ms with --quick), then the best of 7 runs (1 with --quick)
template <class F>
double Measure( F f ) {
	const double target = quick ? 2.0e5 : 2.0e7;
	int n = 1;
	double ns = TimeOnce( f, n );
	while ( ns < target && n < ( 1 << 24 ) ) {
		n *= 2;
		ns = TimeOnce( f, n );
	}
	double best = ns / n;
	for ( int run = 1; run < ( quick ? 1 : 7 ); run++ ) {
		const double t = TimeOnce( f, n ) / n;
		if ( t < best ) {
			best = t;
		}
	}
	return best;
}

void Row( const char *name, const char *what, double nsPerCall, const char *unit = NULL, double count = 0.0 ) {
	if ( unit && count > 0.0 ) {
		printf( "  %-11s %-38s %10.1f ns/call %8.2f ns/%s\n", name, what, nsPerCall, nsPerCall / count, unit );
	} else {
		printf( "  %-11s %-38s %10.1f ns/call\n", name, what, nsPerCall );
	}
}

int Mod0Bones( CGhoul2Info_v &ghoul2 ) {
	model_t *mod = R_GetModelByHandle( RE_RegisterModel( ghoul2[0].mFileName ) );
	return R_GetModelByHandle( mod->mdxm->animIndex )->mdxa->numBones;
}

int InstanceBones( CGhoul2Info_v &ghoul2 ) {
	int bones = 0;
	for ( size_t i = 0; i < ghoul2.size(); i++ ) {
		model_t *mod = R_GetModelByHandle( RE_RegisterModel( ghoul2[i].mFileName ) );
		bones += R_GetModelByHandle( mod->mdxm->animIndex )->mdxa->numBones;
	}
	return bones;
}

void Bench( const char *title, g2handle_t handle ) {
	CGhoul2Info_v &ghoul2 = *G2API_GetGhoul2Model( handle );
	const vec3_t angles = { 0, 0, 0 }, origin = { 0, 0, 0 }, scale = { 0, 0, 0 };
	const int t0 = 20000;

	// the draw surfaces of a LOD 0 frame
	trRefEntity_t ent;
	memset( &ent, 0, sizeof( ent ) );
	ent.e.ghoul2 = handle;
	ent.e.radius = 64.0f;
	g2t::projectedRadius = 0.0f;
	tr.refdef.time = t0;
	R_ResetRenderableSurfaces();
	g2t::ClearDrawSurfs();
	R_AddGhoulSurfaces( &ent );
	const std::vector<g2t::DrawSurf> surfs( g2t::drawSurfs );
	int verts = 0;
	for ( size_t i = 0; i < surfs.size(); i++ ) {
		verts += surfs[i].surf->surfaceData->numVerts;
	}
	const int instanceBones = InstanceBones( ghoul2 );
	printf( "%s: %d model%s, %d bones, %d surfaces drawn, %d vertexes in LOD 0\n",
		title, (int)ghoul2.size(), ghoul2.size() > 1 ? "s" : "", instanceBones, (int)surfs.size(), verts );

	int frame = 0;
	double ns = Measure( [&]( int n ) {
		for ( int i = 0; i < n; i++ ) {
			tr.refdef.time = t0 + 16 * ( ++frame );
			G2_ConstructGhoulSkeleton( ghoul2, tr.refdef.time, NULL, true, angles, origin, scale, false );
		}
	} );
	Row( "skeleton", "G2_ConstructGhoulSkeleton, client", ns, "bone", instanceBones );

	{
		model_t *mod = R_GetModelByHandle( RE_RegisterModel( ghoul2[0].mFileName ) );
		mdxaHeader_t *header = R_GetModelByHandle( mod->mdxm->animIndex )->mdxa;
		const int numBones = Mod0Bones( ghoul2 );
		std::vector<int> used( numBones, 1 );
		mdxaBone_t root = g2t::RotationMatrix( 1.0, 0.0, 0.0, 0.0 );
		ns = Measure( [&]( int n ) {
			for ( int i = 0; i < n; i++ ) {
				G2_TransformGhoulBones( header, &used[0], ghoul2[0].mBlist, ghoul2[0].mTempBoneList, ghoul2[0].mBltlist,
										root, ghoul2[0], t0 + 16 * ( ++frame ), numBones );
			}
		} );
		Row( "bones", "G2_TransformGhoulBones, all bones", ns, "bone", numBones );
	}

	ns = Measure( [&]( int n ) {
		for ( int i = 0; i < n; i++ ) {
			for ( size_t s = 0; s < surfs.size(); s++ ) {
				G2_SkinVertexes( surfs[s].surf->surfaceData, *(const mdxaBone_v *)surfs[s].surf->boneList, g2t::skinXyz, g2t::skinNormal );
			}
		}
		sink += Bits( g2t::skinXyz[0][0] );
	} );
	Row( "skin", "G2_SkinVertexes (portable)", ns, "vertex", verts );

#if id386 || idx64
	ns = Measure( [&]( int n ) {
		for ( int i = 0; i < n; i++ ) {
			for ( size_t s = 0; s < surfs.size(); s++ ) {
				G2_SkinVertexesSSE2( surfs[s].surf->surfaceData, *(const mdxaBone_v *)surfs[s].surf->boneList, g2t::skinXyz, g2t::skinNormal );
			}
		}
		sink += Bits( g2t::skinXyz[0][0] );
	} );
	Row( "skin", "G2_SkinVertexesSSE2", ns, "vertex", verts );
#endif

	ns = Measure( [&]( int n ) {
		for ( int i = 0; i < n; i++ ) {
			for ( size_t s = 0; s < surfs.size(); s++ ) {
				tess.numVertexes = 0;
				tess.numIndexes = 0;
				RB_SurfaceGhoul( surfs[s].surf );
			}
		}
	} );
	Row( "surface", "RB_SurfaceGhoul", ns, "vertex", verts );

	ns = Measure( [&]( int n ) {
		for ( int i = 0; i < n; i++ ) {
			tr.refdef.time = t0 + 16 * ( ++frame );
			R_ResetRenderableSurfaces();
			g2t::ClearDrawSurfs();
			R_AddGhoulSurfaces( &ent );
		}
	} );
	Row( "frontend", "R_AddGhoulSurfaces (with the skeleton)", ns );

	{
		CMiniHeap heap( 4 * 1024 * 1024 );
		int collisionVerts = 0;
		G2_TransformModel( ghoul2, t0, scale, &heap, 0 );
		for ( size_t i = 0; i < ghoul2.size(); i++ ) {
			model_t *mod = R_GetModelByHandle( RE_RegisterModel( ghoul2[i].mFileName ) );
			for ( int s = 0; s < mod->mdxm->numSurfaces; s++ ) {
				if ( ghoul2[i].mTransformedVertsArray[s] ) {
					collisionVerts += ( (const mdxmSurface_t *)G2_FindSurface( mod, s, 0 ) )->numVerts;
				}
			}
		}
		ns = Measure( [&]( int n ) {
			for ( int i = 0; i < n; i++ ) {
				heap.ResetHeap();
				G2_TransformModel( ghoul2, t0 + 16 * ( ++frame ), scale, &heap, 0 );
			}
		} );
		Row( "collision", "G2_TransformModel, LOD 0", ns, "vertex", collisionVerts );
	}
}

void BenchKernels( void ) {
	enum { kPairs = 256 };
	static mdxaBone_t a[kPairs], b[kPairs], out[kPairs];
	uint32_t s = 1;
	for ( int n = 0; n < kPairs; n++ ) {
		float *fa = &a[n].matrix[0][0], *fb = &b[n].matrix[0][0];
		for ( int i = 0; i < 12; i++ ) {
			s = s * 1664525u + 1013904223u;
			fa[i] = (float)( (int)( s >> 16 ) - 32768 ) / 32768.0f;
			s = s * 1664525u + 1013904223u;
			fb[i] = (float)( (int)( s >> 16 ) - 32768 ) / 32768.0f;
		}
	}
	double ns = Measure( [&]( int n ) {
		for ( int i = 0; i < n; i++ ) {
			for ( int k = 0; k < kPairs; k++ ) {
				Multiply_3x4Matrix( &out[k], &a[k], &b[( k + i ) & ( kPairs - 1 )] );
			}
		}
		sink += Bits( out[0].matrix[0][0] );
	} );
	Row( "multiply", "Multiply_3x4Matrix", ns / kPairs );

	model_t *gla = R_GetModelByHandle( g2t::ModelByName( g2t::kCharGLM )->mdxm->animIndex );
	const int frames = gla->mdxa->numFrames, bones = gla->mdxa->numBones;
	ns = Measure( [&]( int n ) {
		mdxaBone_t m;
		for ( int i = 0; i < n; i++ ) {
			for ( int f = 0; f < frames; f++ ) {
				for ( int k = 0; k < bones; k++ ) {
					UnCompressBone( m.matrix, k, gla->mdxa, f );
				}
			}
			sink += Bits( m.matrix[0][0] );
		}
	} );
	Row( "uncompress", "UnCompressBone", ns / ( frames * bones ) );
}

} // namespace

int main( int argc, char **argv ) {
	for ( int i = 1; i < argc; i++ ) {
		if ( !strcmp( argv[i], "--quick" ) ) {
			quick = true;
		}
	}

	g2t::Init();
	g2t::AddSyntheticModels();
	g2t::SetEngineCvars( true );

#if defined( _MSC_VER )
	const char *compiler = "MSVC";
#elif defined( __clang__ )
	const char *compiler = "clang";
#elif defined( __GNUC__ )
	const char *compiler = "GCC";
#else
	const char *compiler = "?";
#endif
#ifdef NDEBUG
	const char *build = "release";
#else
	const char *build = "debug";
#endif
	printf( "Ghoul2 micro-benchmarks (FE-16), %s %s %s, %s\n\n", ARCH_STRING, compiler, build,
		quick ? "quick run, the times mean nothing" : "best of 7 runs of 20 ms" );

	g2handle_t character = 0;
	g2t::Bolts bolts;
	if ( !g2t::SetupCharacter( &character, 20000, &bolts ) ) {
		printf( "could not set up the synthetic character\n" );
		return 1;
	}
	Bench( "synthetic character and prop", character );
	G2API_CleanGhoul2Models( &character );
	printf( "\n" );

	if ( !quick ) {
		const std::string base = g2t::FindRetailBase( argc > 0 ? argv[0] : "" );
		std::vector<std::string> names;
		names.push_back( g2t::kKyleGLM );
		names.push_back( g2t::kHumanoidGLA );
		g2handle_t kyle = 0;
		g2t::Bolts kyleBolts;
		if ( base.empty() || !g2t::LoadFromPk3( base + "/assets0.pk3", names ) ) {
			printf( "Kyle: no retail assets0.pk3 (set JK2MV_TEST_BASE to the folder that holds it)\n\n" );
		} else if ( !g2t::SetupKyle( &kyle, 20000, &kyleBolts ) ) {
			printf( "Kyle: could not set up the model\n\n" );
		} else {
			Bench( "Kyle (retail)", kyle );
			G2API_CleanGhoul2Models( &kyle );
			printf( "\n" );
		}
	}

	BenchKernels();
	printf( "\n(%u)\n", sink & 1 );
	g2t::ResetModels();
	return 0;
}
