// test_ghoul2_cache.cpp - FE-3: the bit-exact caches of the Ghoul2 code
//
// The reference tests (test_ghoul2.cpp) check that the skeletons and draw
// surfaces stay bit for bit what they were; these check when each cache is
// reused and when it is rebuilt, on the synthetic models of the harness:
//  - the used bone list (CGhoul2Info::mBoneUsedCache) holds across frames
//    and animation changes, and is rebuilt when the surfaces, the root
//    surface, the model or the model slots change, each one alone, whatever
//    path builds the skeleton (a bolt, a collision, the renderer);
//  - each instance keeps its own, copies included;
//  - a skeleton built with the caches is bit for bit the one built without;
//  - the skin shader table (CGhoul2Info::mSkinShaderCache) holds across
//    frames and is rebuilt for another skin, the instance's or the entity's,
//    for another model with the same skin, and for new model slots and
//    skins; each instance keeps its own.
//
// A cache hit is told from a rebuild by a mark: the test writes a value the
// cache never holds (2 in a used bone list, a shader no skin names in a skin
// table); a hit keeps it, a rebuild clears it.
//
// Not tested here: that R_ModelInit, R_SVModelInit and R_InitSkins start a
// new generation. The harness does not link tr_model.cpp or tr_image.cpp
// (it has its own model and skin registry), so the tests call
// G2_InvalidateModelCaches where those functions would.

#include <gtest/gtest.h>
#include "ghoul2_harness.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

namespace {

const vec3_t kNoAngles = { 0, 0, 0 };
const vec3_t kNoScale = { 0, 0, 0 };
const int kT0 = 10000;
const int kMark = 2;

class Ghoul2Cache : public ::testing::Test {
protected:
	g2handle_t	handle;
	g2t::Bolts	bolts;
	g2handle_t	handle2;
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

	static CGhoul2Info_v &G2( g2handle_t h ) {
		return *G2API_GetGhoul2Model( h );
	}

	// The character with its bolts and the prop, as SetupCharacter makes
	// it but with no bolt on a generated surface, for the tests that set a
	// root surface: see SetRootSurface
	static bool SetupCharacterForRoot( g2handle_t *h, int t0, g2t::Bolts *b ) {
		return g2t::CreateCharacter( h )
			&& g2t::AnimateCharacter( *h, t0 )
			&& g2t::AddCharacterBolts( *h, b, false )
			&& g2t::AttachProp( *h, t0, b );
	}

	// G2API_SetRootSurface on the character, refused when one of its bolts
	// is on a generated surface: G2_RemoveRedundantBolts (called by
	// G2_SetRootSurface) removes such a bolt (G2_FindOverrideSurface never
	// finds a generated surface), then reads past the end of the bolt list,
	// which is undefined and aborts with checked containers (MSVC Debug).
	// See SurfaceOverridesAndRoot in test_ghoul2.cpp.
	static bool SetRootSurface( g2handle_t h, const char *name ) {
		const boltInfo_v &bltlist = G2( h )[0].mBltlist;
		for ( size_t b = 0; b < bltlist.size(); b++ ) {
			if ( bltlist[b].surfaceNumber != -1 && bltlist[b].surfaceType ) {
				ADD_FAILURE() << "bolt " << b << " is on a generated surface";
				return false;
			}
		}
		return G2API_SetRootSurface( h, 0, name ) != qfalse;
	}

	// the skeleton build the game and cgame trigger through a bolt
	static void BuildThroughBolt( g2handle_t h, int bolt, int time ) {
		mdxaBone_t m;
		ASSERT_TRUE( G2API_GetBoltMatrix( h, 0, bolt, &m, kNoAngles, vec3_origin, time, NULL, kNoScale ) );
		EXPECT_EQ( time, G2( h )[0].mSkelFrameNum );
	}

	// the one R_AddGhoulSurfaces runs (after a bolt in the same frame, it
	// reuses that skeleton)
	static void Render( g2handle_t h, int time, qhandle_t customSkin = 0, bool clear = true ) {
		tr.refdef.time = time;
		if ( clear ) {
			R_ResetRenderableSurfaces();
			g2t::ClearDrawSurfs();
		}
		trRefEntity_t ent;
		memset( &ent, 0, sizeof( ent ) );
		ent.e.ghoul2 = h;
		ent.e.radius = 64.0f;
		ent.e.customSkin = customSkin;
		R_AddGhoulSurfaces( &ent );
		EXPECT_EQ( time, G2( h )[0].mSkelFrameNum );
	}

	// The index of a surface of model i of the instance
	static int Surface( g2handle_t h, size_t i, const char *name ) {
		int flags = 0;
		return G2_IsSurfaceLegal( R_GetModelByHandle( RE_RegisterModel( G2( h )[i].mFileName ) ), name, &flags );
	}

	// The shader of the draw surface of a surface of model i of the
	// instance, in the last frame drawn
	static std::string Drawn( g2handle_t h, size_t i, int surface ) {
		const void *bones = &G2( h )[i].mTempBoneList;
		for ( size_t d = 0; d < g2t::drawSurfs.size(); d++ ) {
			const g2t::DrawSurf &ds = g2t::drawSurfs[d];
			if ( ds.surf->boneList == bones && ds.surf->surfaceData->thisSurfaceIndex == surface ) {
				return ds.shader ? ds.shader->name : "<none>";
			}
		}
		return "<not drawn>";
	}

	// The shader the model file gives a surface of model i of the instance,
	// the one drawn without a skin or a custom shader
	static std::string ModelShader( g2handle_t h, size_t i, int surface ) {
		const model_t *mod = R_GetModelByHandle( RE_RegisterModel( G2( h )[i].mFileName ) );
		const mdxmHierarchyOffsets_t *surfIndexes = (const mdxmHierarchyOffsets_t *)( (const char *)mod->mdxm + sizeof( mdxmHeader_t ) );
		const mdxmSurfHierarchy_t *surfInfo = (const mdxmSurfHierarchy_t *)( (const char *)surfIndexes + surfIndexes->offsets[surface] );
		return R_GetShaderByHandle( surfInfo->shaderIndex )->name;
	}

	static shader_t *Shader( const char *name ) {
		return R_GetShaderByHandle( g2t::ShaderHandle( name ) );
	}

	// A skin of surface and shader name pairs
	static qhandle_t Skin( const char *name, const char *const *pairs, int count ) {
		return g2t::RegisterSkin( name, std::vector<std::string>( pairs, pairs + count ) );
	}

	// the one of a collision test (always rebuilt)
	static void Collide( g2handle_t h, int time ) {
		CollisionRecord_t records[MAX_G2_COLLISIONS];
		memset( records, 0, sizeof( records ) );
		for ( int i = 0; i < MAX_G2_COLLISIONS; i++ ) {
			records[i].mEntityNum = -1;
		}
		CMiniHeap heap( 2 * 1024 * 1024 );
		const vec3_t start = { 80.0f, 0.0f, 50.0f }, end = { -60.0f, 0.0f, 50.0f };
		G2API_CollisionDetect( records, h, kNoAngles, vec3_origin, time, 1, start, end, kNoScale, &heap, G2_COLLIDE, 0, 0.0f );
	}

	// Marks the used bone list of each model of the instance (a mark left
	// from before goes first)
	static void Mark( g2handle_t h ) {
		CGhoul2Info_v &ghoul2 = G2( h );
		for ( size_t i = 0; i < ghoul2.size(); i++ ) {
			Hit( h, i );
			std::vector<int> &used = ghoul2[i].mBoneUsedCache;
			ASSERT_FALSE( used.empty() );
			size_t b = 0;
			while ( b < used.size() && used[b] != 1 ) {
				b++;
			}
			ASSERT_LT( b, used.size() );
			used[b] = kMark;
		}
	}

	// Whether model i kept its mark (then clears it)
	static bool Hit( g2handle_t h, size_t i ) {
		std::vector<int> &used = G2( h )[i].mBoneUsedCache;
		for ( size_t b = 0; b < used.size(); b++ ) {
			if ( used[b] == kMark ) {
				used[b] = 1;
				return true;
			}
		}
		return false;
	}

	// The used bone lists of the instance as a build without a cache makes
	// them: the same skeleton build on a copy whose caches are invalid
	static std::vector<std::vector<int> > Fresh( g2handle_t h, int time ) {
		CGhoul2Info_v copy = G2( h );
		for ( size_t i = 0; i < copy.size(); i++ ) {
			copy[i].mBoneUsedGeneration = -1;
			copy[i].mBoneUsedCache.clear();
		}
		G2_ConstructGhoulSkeleton( copy, time, NULL, true, kNoAngles, vec3_origin, kNoScale, false );
		std::vector<std::vector<int> > lists;
		for ( size_t i = 0; i < copy.size(); i++ ) {
			lists.push_back( copy[i].mBoneUsedCache );
		}
		return lists;
	}

	// The used bone lists of the instance, without the marks
	static std::vector<std::vector<int> > Cached( g2handle_t h ) {
		std::vector<std::vector<int> > lists;
		CGhoul2Info_v &ghoul2 = G2( h );
		for ( size_t i = 0; i < ghoul2.size(); i++ ) {
			lists.push_back( ghoul2[i].mBoneUsedCache );
			for ( size_t b = 0; b < lists.back().size(); b++ ) {
				if ( lists.back()[b] == kMark ) {
					lists.back()[b] = 1;
				}
			}
		}
		return lists;
	}

	static int Count( const std::vector<int> &used ) {
		int n = 0;
		for ( size_t b = 0; b < used.size(); b++ ) {
			n += used[b] != 0;
		}
		return n;
	}
};

// One instance through frames, animation changes, surface changes, a new
// root surface, a direct edit of the surface list and new model slots
TEST_F( Ghoul2Cache, UsedBonesFollowTheSurfaces ) {
	ASSERT_TRUE( SetupCharacterForRoot( &handle, kT0, &bolts ) );
	BuildThroughBolt( handle, bolts.rHand, kT0 );
	const std::vector<std::vector<int> > whole = Cached( handle );
	ASSERT_EQ( 2u, whole.size() );
	EXPECT_EQ( Fresh( handle, kT0 ), whole );

	// a new frame: reused, by each path that builds a skeleton
	Mark( handle );
	BuildThroughBolt( handle, bolts.rHand, kT0 + 16 );
	EXPECT_TRUE( Hit( handle, 0 ) );
	EXPECT_TRUE( Hit( handle, 1 ) );
	Mark( handle );
	Render( handle, kT0 + 32 );
	EXPECT_TRUE( Hit( handle, 0 ) );
	EXPECT_TRUE( Hit( handle, 1 ) );
	Mark( handle );
	Collide( handle, kT0 + 48 );
	EXPECT_TRUE( Hit( handle, 0 ) );
	EXPECT_TRUE( Hit( handle, 1 ) );
	EXPECT_EQ( whole, Cached( handle ) );

	// new animations and bone angles: the surfaces use the same bones
	ASSERT_TRUE( G2API_SetBoneAnim( handle, 0, "model_root", 20, 44, BONE_ANIM_OVERRIDE_LOOP, 1.5f, kT0 + 50, -1, 100 ) );
	ASSERT_TRUE( G2API_SetBoneAngles( handle, 0, "cranium", kNoAngles, BONE_ANGLES_POSTMULT, POSITIVE_Z, NEGATIVE_Y, POSITIVE_X, NULL, 0, kT0 + 50 ) );
	ASSERT_TRUE( G2API_SetBoneAnim( handle, 1, "prop_root", 1, 6, BONE_ANIM_OVERRIDE_LOOP, 0.5f, kT0 + 50, -1, -1 ) );
	Mark( handle );
	BuildThroughBolt( handle, bolts.rHand, kT0 + 64 );
	EXPECT_TRUE( Hit( handle, 0 ) );
	EXPECT_TRUE( Hit( handle, 1 ) );

	// surfaces off: no surface uses the left leg bones any more
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "cloak", G2SURFACEFLAG_OFF ) );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "l_leg", G2SURFACEFLAG_OFF | G2SURFACEFLAG_NODESCENDANTS ) );
	Mark( handle );
	Render( handle, kT0 + 80 );
	EXPECT_FALSE( Hit( handle, 0 ) );
	EXPECT_TRUE( Hit( handle, 1 ) );	// the prop's surfaces did not change
	const std::vector<std::vector<int> > noLeg = Cached( handle );
	EXPECT_LT( Count( noLeg[0] ), Count( whole[0] ) );
	EXPECT_EQ( Fresh( handle, kT0 + 80 ), noLeg );

	// back on: the overrides stay in the list, with other flags
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "l_leg", 0 ) );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "cloak", 0 ) );
	Mark( handle );
	BuildThroughBolt( handle, bolts.rHand, kT0 + 96 );
	EXPECT_FALSE( Hit( handle, 0 ) );
	EXPECT_EQ( whole, Cached( handle ) );

	// overrides removed (G2API_RemoveSurface), the cloak and the left leg
	// off. G2_RemoveSurface clears the surface number of the entry, keeps
	// its flags, and drops the cleared entries at the end of the list: the
	// last one removed shortens the list, the first one keeps its length.
	surfaceInfo_v &slist = G2( handle )[0].mSlist;
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "cloak", G2SURFACEFLAG_OFF ) );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "l_leg", G2SURFACEFLAG_OFF | G2SURFACEFLAG_NODESCENDANTS ) );
	ASSERT_EQ( 2u, slist.size() );
	BuildThroughBolt( handle, bolts.rHand, kT0 + 100 );
	EXPECT_EQ( noLeg, Cached( handle ) );
	ASSERT_TRUE( G2API_RemoveSurface( &G2( handle )[0], 1 ) );	// the left leg
	ASSERT_EQ( 1u, slist.size() );
	Mark( handle );
	BuildThroughBolt( handle, bolts.rHand, kT0 + 104 );
	EXPECT_FALSE( Hit( handle, 0 ) );
	EXPECT_NE( noLeg, Cached( handle ) );	// the leg uses its bones again
	EXPECT_EQ( Fresh( handle, kT0 + 104 ), Cached( handle ) );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "l_leg", G2SURFACEFLAG_OFF | G2SURFACEFLAG_NODESCENDANTS ) );
	BuildThroughBolt( handle, bolts.rHand, kT0 + 108 );
	EXPECT_EQ( noLeg, Cached( handle ) );
	ASSERT_TRUE( G2API_RemoveSurface( &G2( handle )[0], 0 ) );	// the cloak
	ASSERT_EQ( 2u, slist.size() );
	Mark( handle );
	Render( handle, kT0 + 110 );
	EXPECT_FALSE( Hit( handle, 0 ) );
	EXPECT_NE( noLeg, Cached( handle ) );	// the cloak uses the leg bones again
	EXPECT_EQ( Fresh( handle, kT0 + 110 ), Cached( handle ) );

	// another root surface (it drops the overrides outside it), the cloak
	// off: no surface uses a leg bone
	ASSERT_TRUE( SetRootSurface( handle, "torso" ) );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "cloak", G2SURFACEFLAG_OFF ) );
	Mark( handle );
	Collide( handle, kT0 + 112 );
	EXPECT_FALSE( Hit( handle, 0 ) );
	const std::vector<std::vector<int> > torso = Cached( handle );
	EXPECT_LT( Count( torso[0] ), Count( whole[0] ) );
	EXPECT_EQ( Fresh( handle, kT0 + 112 ), torso );

	// the surface list written directly, as a saved game load does: the
	// cloak on again
	ASSERT_EQ( 1u, slist.size() );
	ASSERT_EQ( Surface( handle, 0, "cloak" ), slist[0].surface );
	const surfaceInfo_t saved = slist[0];
	slist[0].offFlags ^= G2SURFACEFLAG_OFF;
	Mark( handle );
	BuildThroughBolt( handle, bolts.rHand, kT0 + 128 );
	EXPECT_FALSE( Hit( handle, 0 ) );
	EXPECT_NE( torso, Cached( handle ) );
	EXPECT_EQ( Fresh( handle, kT0 + 128 ), Cached( handle ) );
	slist[0] = saved;
	Mark( handle );
	BuildThroughBolt( handle, bolts.rHand, kT0 + 144 );
	EXPECT_FALSE( Hit( handle, 0 ) );
	EXPECT_EQ( torso, Cached( handle ) );

	// new model slots (the call R_ModelInit makes): every list is rebuilt
	G2_InvalidateModelCaches();
	Mark( handle );
	BuildThroughBolt( handle, bolts.rHand, kT0 + 160 );
	EXPECT_FALSE( Hit( handle, 0 ) );
	EXPECT_FALSE( Hit( handle, 1 ) );
	EXPECT_EQ( torso, Cached( handle ) );
}

// Another root surface with no surface override: the surface list stays
// empty, so only the root surface tells the list is stale
TEST_F( Ghoul2Cache, UsedBonesFollowTheRootSurface ) {
	ASSERT_TRUE( g2t::CreateCharacter( &handle ) );
	ASSERT_TRUE( g2t::AnimateCharacter( handle, kT0 ) );
	ASSERT_TRUE( g2t::AddCharacterBolts( handle, &bolts, false ) );
	BuildThroughBolt( handle, bolts.lumbar, kT0 );
	const std::vector<std::vector<int> > whole = Cached( handle );
	const surfaceInfo_v &slist = G2( handle )[0].mSlist;
	ASSERT_TRUE( slist.empty() );

	// the head: its bones and the spine below them (the bolt on the lower
	// lumbar stays)
	ASSERT_TRUE( SetRootSurface( handle, "head" ) );
	ASSERT_TRUE( slist.empty() );
	Mark( handle );
	BuildThroughBolt( handle, bolts.lumbar, kT0 + 16 );
	EXPECT_FALSE( Hit( handle, 0 ) );
	const std::vector<std::vector<int> > head = Cached( handle );
	EXPECT_LT( Count( head[0] ), Count( whole[0] ) );
	EXPECT_EQ( Fresh( handle, kT0 + 16 ), head );

	// the whole body again
	ASSERT_TRUE( SetRootSurface( handle, "hips" ) );
	ASSERT_TRUE( slist.empty() );
	Mark( handle );
	Render( handle, kT0 + 32 );
	EXPECT_FALSE( Hit( handle, 0 ) );
	EXPECT_EQ( whole, Cached( handle ) );

	// the root surface out to the torso and back with no build in between,
	// the cloak and the left leg off, the head on: the root surface is the
	// same again, but G2_SetRootSurface dropped the overrides of the
	// surfaces it found off or outside the torso (the entries keep their
	// flags). With the head last, the list keeps its length and only
	// surface numbers changed.
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "cloak", G2SURFACEFLAG_OFF ) );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "l_leg", G2SURFACEFLAG_OFF | G2SURFACEFLAG_NODESCENDANTS ) );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "head", 0 ) );
	BuildThroughBolt( handle, bolts.lumbar, kT0 + 48 );
	const std::vector<std::vector<int> > noLeg = Cached( handle );
	EXPECT_NE( whole, noLeg );
	ASSERT_TRUE( SetRootSurface( handle, "torso" ) );
	ASSERT_TRUE( SetRootSurface( handle, "hips" ) );
	ASSERT_EQ( 3u, slist.size() );
	Mark( handle );
	BuildThroughBolt( handle, bolts.lumbar, kT0 + 64 );
	EXPECT_FALSE( Hit( handle, 0 ) );
	EXPECT_EQ( whole, Cached( handle ) );

	// again with the head first: the list is shorter, and what is left of
	// it is the start of the list the bones were last listed for
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "cloak", G2SURFACEFLAG_OFF ) );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "l_leg", G2SURFACEFLAG_OFF | G2SURFACEFLAG_NODESCENDANTS ) );
	ASSERT_EQ( 5u, slist.size() );
	BuildThroughBolt( handle, bolts.lumbar, kT0 + 80 );
	EXPECT_EQ( noLeg, Cached( handle ) );
	ASSERT_TRUE( SetRootSurface( handle, "torso" ) );
	ASSERT_TRUE( SetRootSurface( handle, "hips" ) );
	ASSERT_EQ( 3u, slist.size() );
	Mark( handle );
	Collide( handle, kT0 + 96 );
	EXPECT_FALSE( Hit( handle, 0 ) );
	EXPECT_EQ( whole, Cached( handle ) );
}

// Every surface off from the root surface down: no surface uses a bone,
// but with the first surface of the model as its root surface, the list
// still holds the root bone, which the skeleton build transforms as with
// every surface on. Fresh() runs the same code, so this checks the list
// against the bones it must hold.
TEST_F( Ghoul2Cache, UsedBonesKeepTheRootBone ) {
	ASSERT_TRUE( g2t::CreateCharacter( &handle ) );
	ASSERT_TRUE( g2t::AnimateCharacter( handle, kT0 ) );
	ASSERT_TRUE( g2t::AddCharacterBolts( handle, &bolts, false ) );
	ASSERT_TRUE( g2t::CreateCharacter( &handle2 ) );
	ASSERT_TRUE( g2t::AnimateCharacter( handle2, kT0 ) );
	ASSERT_TRUE( g2t::AddCharacterBolts( handle2, &bolts2, false ) );
	ASSERT_EQ( 0, G2( handle )[0].mSurfaceRoot );
	ASSERT_EQ( 0, Surface( handle, 0, "hips" ) );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle, "hips", G2SURFACEFLAG_OFF | G2SURFACEFLAG_NODESCENDANTS ) );
	std::vector<int> rootOnly( g2t::kCharBones, 0 );
	rootOnly[0] = 1;

	// on a server (no smoothing) a bone only depends on its parents: the
	// root bone of the second instance, every surface on, is the reference
	BuildThroughBolt( handle, bolts.lumbar, kT0 );
	BuildThroughBolt( handle2, bolts2.lumbar, kT0 );
	EXPECT_EQ( rootOnly, G2( handle )[0].mBoneUsedCache );
	ASSERT_FALSE( G2( handle )[0].mTempBoneList.empty() );
	ASSERT_FALSE( G2( handle2 )[0].mTempBoneList.empty() );
	const mdxaBone_t &root = G2( handle )[0].mTempBoneList[0].second;
	const mdxaBone_t &reference = G2( handle2 )[0].mTempBoneList[0].second;
	EXPECT_EQ( 0, memcmp( &reference, &root, sizeof( root ) ) );

	// and so it stays in the next frames
	Mark( handle );
	BuildThroughBolt( handle, bolts.lumbar, kT0 + 16 );
	EXPECT_TRUE( Hit( handle, 0 ) );
	EXPECT_EQ( rootOnly, G2( handle )[0].mBoneUsedCache );
}

// Another model in the slot of a removed one (G2API_InitGhoul2Model takes
// the first free slot and keeps its root surface): a second file of the
// same skeleton and surfaces, drawn with the same skin, the surface list
// empty before and after. Only the model tells the bone list and the skin
// table are stale.
TEST_F( Ghoul2Cache, CachesFollowTheModel ) {
	static const char *const kCopyGLM = "models/test/g2char_copy.glm";
	g2t::AddFile( kCopyGLM, g2t::BuildCharGLM() );
	ASSERT_TRUE( g2t::CreateCharacter( &handle ) );
	ASSERT_TRUE( g2t::AnimateCharacter( handle, kT0 ) );
	// the prop after it, not bolted to it: the slot of the character stays
	// in the instance when the character goes
	ASSERT_EQ( 1, G2API_InitGhoul2Model( &handle, g2t::kPropGLM, 2 ) );
	static const char *const bluePairs[] = { "torso", "skins/blue_torso", "head", "skins/blue_head" };
	const qhandle_t blue = Skin( "skins/blue", bluePairs, 4 );
	const int torso = Surface( handle, 0, "torso" );
	ASSERT_GE( torso, 0 );

	Render( handle, kT0, blue );
	EXPECT_EQ( "skins/blue_torso", Drawn( handle, 0, torso ) );
	const std::vector<std::vector<int> > before = Cached( handle );
	ASSERT_TRUE( G2( handle )[0].mSlist.empty() );

	ASSERT_TRUE( G2API_RemoveGhoul2Model( &handle, 0 ) );
	ASSERT_EQ( 2u, G2( handle ).size() );
	ASSERT_EQ( 0, G2API_InitGhoul2Model( &handle, kCopyGLM, 1 ) );
	ASSERT_NE( g2t::ModelByName( g2t::kCharGLM ), g2t::ModelByName( kCopyGLM ) );
	ASSERT_TRUE( G2( handle )[0].mSlist.empty() );
	ASSERT_EQ( 0, G2( handle )[0].mSurfaceRoot );
	Mark( handle );
	G2( handle )[0].mSkinShaderCache[torso] = Shader( "test/mark" );
	Render( handle, kT0 + 16, blue );
	EXPECT_FALSE( Hit( handle, 0 ) );
	EXPECT_TRUE( Hit( handle, 1 ) );
	EXPECT_EQ( before, Cached( handle ) );	// the same bones as the first file
	EXPECT_EQ( Fresh( handle, kT0 + 16 ), Cached( handle ) );
	EXPECT_EQ( "skins/blue_torso", Drawn( handle, 0, torso ) );
}

// Two instances of the same model with different surfaces, built in turn
// in each frame, and a copy of one changed afterwards: each keeps its own
TEST_F( Ghoul2Cache, UsedBonesPerInstance ) {
	ASSERT_TRUE( g2t::SetupCharacter( &handle, kT0, &bolts ) );
	ASSERT_TRUE( g2t::CreateCharacter( &handle2 ) );
	ASSERT_TRUE( g2t::AnimateCharacter( handle2, kT0 - 270 ) );
	ASSERT_TRUE( g2t::AddCharacterBolts( handle2, &bolts2, false ) );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle2, "cloak", G2SURFACEFLAG_OFF ) );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( handle2, "l_leg", G2SURFACEFLAG_OFF | G2SURFACEFLAG_NODESCENDANTS ) );

	BuildThroughBolt( handle, bolts.rHand, kT0 );
	BuildThroughBolt( handle2, bolts2.lHand, kT0 );
	const std::vector<std::vector<int> > first = Cached( handle ), second = Cached( handle2 );
	EXPECT_NE( first[0], second[0] );
	EXPECT_EQ( Fresh( handle, kT0 ), first );
	EXPECT_EQ( Fresh( handle2, kT0 ), second );

	for ( int k = 1; k < 6; k++ ) {
		const int t = kT0 + 16 * k;
		Mark( handle );
		Mark( handle2 );
		if ( k & 1 ) {
			Render( handle2, t );
			Render( handle, t );
		} else {
			BuildThroughBolt( handle, bolts.rHand, t );
			BuildThroughBolt( handle2, bolts2.lHand, t );
		}
		EXPECT_TRUE( Hit( handle, 0 ) );
		EXPECT_TRUE( Hit( handle, 1 ) );
		EXPECT_TRUE( Hit( handle2, 0 ) );
		EXPECT_EQ( first, Cached( handle ) );
		EXPECT_EQ( second, Cached( handle2 ) );
	}

	// a copy starts with the lists of its source, then goes its own way
	g2handle_t copy = 0;
	G2API_DuplicateGhoul2Instance( handle, &copy );
	ASSERT_NE( 0, copy );
	EXPECT_EQ( first, Cached( copy ) );
	ASSERT_TRUE( G2API_SetSurfaceOnOff( copy, "cloak", G2SURFACEFLAG_OFF ) );
	Mark( handle );
	Mark( copy );
	BuildThroughBolt( copy, bolts.rHand, kT0 + 96 );
	BuildThroughBolt( handle, bolts.rHand, kT0 + 96 );
	EXPECT_TRUE( Hit( handle, 0 ) );
	EXPECT_FALSE( Hit( copy, 0 ) );
	EXPECT_TRUE( Hit( copy, 1 ) );
	EXPECT_EQ( first, Cached( handle ) );
	EXPECT_EQ( Fresh( copy, kT0 + 96 ), Cached( copy ) );
	G2API_CleanGhoul2Models( &copy );
}

// Two identical instances driven the same way on a client (bone smoothing
// and unsquash, so each skeleton depends on every build before it), the
// second without its caches: their bones and bolts stay the same bit for
// bit, stale bones included, through surface and animation changes. Each
// surface change changes the used bones, and three keep the length of the
// surface list, so that a cache that only looked at the length would fall
// behind: the flags of an override (frame 5), an override removed from the
// middle of the list (11) and the root surface alone (15).
TEST_F( Ghoul2Cache, SameBonesWithoutCache ) {
	g2t::SetEngineCvars( true );
	ASSERT_TRUE( SetupCharacterForRoot( &handle, kT0, &bolts ) );
	ASSERT_TRUE( SetupCharacterForRoot( &handle2, kT0, &bolts2 ) );
	const g2handle_t both[2] = { handle, handle2 };
	std::vector<int> lastUsed;
	size_t lastLength = 0;

	for ( int k = 0; k < 24; k++ ) {
		for ( int e = 0; e < 2; e++ ) {
			const g2handle_t h = both[e];
			if ( k == 3 ) {
				// no surface uses the left leg bones
				ASSERT_TRUE( G2API_SetSurfaceOnOff( h, "cloak", G2SURFACEFLAG_OFF ) );
				ASSERT_TRUE( G2API_SetSurfaceOnOff( h, "l_leg", G2SURFACEFLAG_OFF | G2SURFACEFLAG_NODESCENDANTS ) );
			} else if ( k == 5 ) {
				// the cloak uses them again
				ASSERT_TRUE( G2API_SetSurfaceOnOff( h, "cloak", 0 ) );
			} else if ( k == 7 ) {
				ASSERT_TRUE( G2API_SetBoneAnim( h, 0, "lower_lumbar", 4, 20, BONE_ANIM_OVERRIDE_LOOP | BONE_ANIM_BLEND, 1.1f, kT0 + 16 * k, -1, 80 ) );
			} else if ( k == 9 ) {
				// no leg bone (the left leg override goes with the root)
				ASSERT_TRUE( SetRootSurface( h, "torso" ) );
				ASSERT_TRUE( G2API_SetSurfaceOnOff( h, "cloak", G2SURFACEFLAG_OFF ) );
			} else if ( k == 10 ) {
				// no cranium
				ASSERT_TRUE( G2API_SetSurfaceOnOff( h, "head", G2SURFACEFLAG_OFF ) );
			} else if ( k == 11 ) {
				// the cloak override removed: the cloak is on again
				ASSERT_EQ( Surface( h, 0, "cloak" ), G2( h )[0].mSlist[0].surface );
				ASSERT_TRUE( G2API_RemoveSurface( &G2( h )[0], 0 ) );
			} else if ( k == 13 ) {
				// the right arm (and the prop on its hand); the head
				// override goes
				ASSERT_TRUE( SetRootSurface( h, "r_arm" ) );
			} else if ( k == 15 ) {
				ASSERT_TRUE( SetRootSurface( h, "hips" ) );
			} else if ( k == 19 ) {
				ASSERT_TRUE( G2API_SetSurfaceOnOff( h, "head", G2SURFACEFLAG_OFF ) );
				ASSERT_TRUE( G2API_SetSurfaceOnOff( h, "head_cap_torso_off", 0 ) );
			}
		}
		const int t = kT0 + 16 * k;
		tr.refdef.time = t;	// the time stamps of the smoothing
		for ( int e = 0; e < 2; e++ ) {
			if ( e == 1 ) {
				CGhoul2Info_v &ghoul2 = G2( handle2 );
				for ( size_t i = 0; i < ghoul2.size(); i++ ) {
					ghoul2[i].mBoneUsedGeneration = -1;
				}
			}
			if ( k % 3 == 0 ) {
				BuildThroughBolt( both[e], e ? bolts2.rHand : bolts.rHand, t );
				Render( both[e], t );
			} else if ( k % 3 == 1 ) {
				Render( both[e], t );
			} else {
				Collide( both[e], t );
			}
		}

		// the change made the instance without caches use other bones
		const CGhoul2Info &uncached = G2( handle2 )[0];
		if ( k == 3 || k == 5 || k == 9 || k == 10 || k == 11 || k == 13 || k == 15 ) {
			EXPECT_NE( lastUsed, uncached.mBoneUsedCache ) << "frame " << k;
		}
		if ( k == 5 || k == 11 || k == 15 ) {
			EXPECT_EQ( lastLength, uncached.mSlist.size() ) << "frame " << k;
		}
		lastUsed = uncached.mBoneUsedCache;
		lastLength = uncached.mSlist.size();

		char an[64], bn[64];
		sprintf( an, "cache/%02d/cached", k );
		sprintf( bn, "cache/%02d/uncached", k );
		g2t::Digest a( an ), b( bn );
		g2t::HashBones( a, G2( handle ) );
		g2t::HashBolts( a, G2( handle ) );
		g2t::HashBones( b, G2( handle2 ) );
		g2t::HashBolts( b, G2( handle2 ) );
		EXPECT_EQ( b.Value(), a.Value() ) << "frame " << k;
	}
}

// The skin shader table of an instance: filled by the frames that draw the
// surfaces, reused by the next ones, rebuilt for another skin (the
// instance's, the entity's) and for new model slots and skins
TEST_F( Ghoul2Cache, SkinShadersFollowTheSkin ) {
	ASSERT_TRUE( g2t::SetupCharacter( &handle, kT0, &bolts ) );
	static const char *const bluePairs[] = { "torso", "skins/blue_torso", "head", "skins/blue_head" };
	static const char *const redPairs[] = { "torso", "skins/red_torso" };
	static const char *const bladePairs[] = { "blade", "skins/blue_blade" };
	const qhandle_t blue = Skin( "skins/blue", bluePairs, 4 );
	const qhandle_t red = Skin( "skins/red", redPairs, 2 );
	const qhandle_t blade = Skin( "skins/blade", bladePairs, 2 );
	const int torso = Surface( handle, 0, "torso" ), head = Surface( handle, 0, "head" );
	const int hips = Surface( handle, 0, "hips" ), bladeSurface = Surface( handle, 1, "blade" );
	ASSERT_GE( torso, 0 );
	ASSERT_GE( head, 0 );
	ASSERT_GE( hips, 0 );
	ASSERT_GE( bladeSurface, 0 );
	shader_t *const mark = Shader( "test/mark" );
	std::vector<void *> &table = G2( handle )[0].mSkinShaderCache;

	// no skin, no table: the shaders of the model file
	Render( handle, kT0 );
	EXPECT_TRUE( table.empty() );
	ASSERT_NE( "<default>", ModelShader( handle, 0, torso ) );
	EXPECT_EQ( ModelShader( handle, 0, torso ), Drawn( handle, 0, torso ) );
	EXPECT_EQ( ModelShader( handle, 0, hips ), Drawn( handle, 0, hips ) );

	ASSERT_TRUE( G2API_SetSkin( &G2( handle )[0], blue ) );
	Render( handle, kT0 + 16 );
	EXPECT_EQ( "skins/blue_torso", Drawn( handle, 0, torso ) );
	EXPECT_EQ( "skins/blue_head", Drawn( handle, 0, head ) );
	EXPECT_EQ( "<default>", Drawn( handle, 0, hips ) );
	ASSERT_GT( table.size(), (size_t)torso );
	EXPECT_EQ( (void *)Shader( "skins/blue_torso" ), table[torso] );
	EXPECT_EQ( (void *)tr.defaultShader, table[hips] );

	// reused by the next frame
	table[torso] = mark;
	Render( handle, kT0 + 32 );
	EXPECT_EQ( "test/mark", Drawn( handle, 0, torso ) );
	EXPECT_EQ( "skins/blue_head", Drawn( handle, 0, head ) );

	// another skin: the table starts again
	ASSERT_TRUE( G2API_SetSkin( &G2( handle )[0], red ) );
	Render( handle, kT0 + 48 );
	EXPECT_EQ( "skins/red_torso", Drawn( handle, 0, torso ) );
	EXPECT_EQ( "<default>", Drawn( handle, 0, head ) );
	ASSERT_TRUE( G2API_SetSkin( &G2( handle )[0], blue ) );
	Render( handle, kT0 + 64 );
	EXPECT_EQ( "skins/blue_torso", Drawn( handle, 0, torso ) );

	// the entity's skin, for the prop (the character keeps its own)
	EXPECT_TRUE( G2( handle )[1].mSkinShaderCache.empty() );
	Render( handle, kT0 + 80, blade );
	EXPECT_EQ( "skins/blue_blade", Drawn( handle, 1, bladeSurface ) );
	EXPECT_EQ( "skins/blue_torso", Drawn( handle, 0, torso ) );
	G2( handle )[1].mSkinShaderCache[bladeSurface] = mark;
	Render( handle, kT0 + 96, blade );
	EXPECT_EQ( "test/mark", Drawn( handle, 1, bladeSurface ) );
	Render( handle, kT0 + 112, red );		// another entity skin
	EXPECT_EQ( "<default>", Drawn( handle, 1, bladeSurface ) );
	Render( handle, kT0 + 128, blade );
	EXPECT_EQ( "skins/blue_blade", Drawn( handle, 1, bladeSurface ) );

	// new model slots and skins (the call R_InitSkins and R_ModelInit make)
	table[torso] = mark;
	G2_InvalidateModelCaches();
	Render( handle, kT0 + 144 );
	EXPECT_EQ( "skins/blue_torso", Drawn( handle, 0, torso ) );
}

// Two instances of one model with their own skin, drawn in the same frame
// in both orders: each keeps its own table
TEST_F( Ghoul2Cache, SkinShadersPerInstance ) {
	ASSERT_TRUE( g2t::SetupCharacter( &handle, kT0, &bolts ) );
	ASSERT_TRUE( g2t::CreateCharacter( &handle2 ) );
	ASSERT_TRUE( g2t::AnimateCharacter( handle2, kT0 - 270 ) );
	static const char *const bluePairs[] = { "torso", "skins/blue_torso", "head", "skins/blue_head" };
	static const char *const redPairs[] = { "torso", "skins/red_torso", "head", "skins/red_head" };
	ASSERT_TRUE( G2API_SetSkin( &G2( handle )[0], Skin( "skins/blue", bluePairs, 4 ) ) );
	ASSERT_TRUE( G2API_SetSkin( &G2( handle2 )[0], Skin( "skins/red", redPairs, 4 ) ) );
	const int torso = Surface( handle, 0, "torso" ), head = Surface( handle, 0, "head" );
	ASSERT_GE( torso, 0 );
	ASSERT_GE( head, 0 );

	for ( int k = 0; k < 4; k++ ) {
		const int t = kT0 + 16 * k;
		const g2handle_t first = ( k & 1 ) ? handle2 : handle, second = ( k & 1 ) ? handle : handle2;
		Render( first, t );
		Render( second, t, 0, false );
		EXPECT_EQ( "skins/blue_torso", Drawn( handle, 0, torso ) );
		EXPECT_EQ( "skins/blue_head", Drawn( handle, 0, head ) );
		EXPECT_EQ( "skins/red_torso", Drawn( handle2, 0, torso ) );
		EXPECT_EQ( "skins/red_head", Drawn( handle2, 0, head ) );
	}

	// a mark in the table of the second stays there
	G2( handle2 )[0].mSkinShaderCache[head] = Shader( "test/mark" );
	Render( handle2, kT0 + 64 );
	Render( handle, kT0 + 64, 0, false );
	EXPECT_EQ( "skins/blue_head", Drawn( handle, 0, head ) );
	EXPECT_EQ( "test/mark", Drawn( handle2, 0, head ) );
}

} // namespace

int main( int argc, char **argv ) {
	::testing::InitGoogleTest( &argc, argv );
	return RUN_ALL_TESTS();
}
