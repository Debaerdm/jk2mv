// test_tr_dlightselect.cpp - which dynamic lights a crowded scene keeps
// (r_dlightPriority)
//
// Links the real engine source (src/renderer/tr_dlightselect.cpp), which
// has no engine dependency.

#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <set>
#include <vector>
#include "renderer/tr_dlightselect.h"

namespace {

const int kMaxDlights = 32;		// MAX_DLIGHTS

// the scene's view, at the origin looking along +x
dlselView_t View( bool cullBehind = true ) {
	dlselView_t view = { { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, cullBehind };
	return view;
}

dlselLight_t Light( float x, float y, float z, float radius, float brightness = 1.0f ) {
	dlselLight_t dl = { { x, y, z }, radius, brightness };
	return dl;
}

std::vector<int> Select( const std::vector<dlselLight_t> &lights, dlselHistory_t *history = nullptr,
	const std::vector<dlselView_t> &views = std::vector<dlselView_t>( 1, View() ) ) {
	std::vector<int> chosen( DLSEL_MAX_KEEP, -1 );
	const int n = R_DlightSelect( lights.data(), (int)lights.size(), kMaxDlights, views.data(), (int)views.size(),
		history, chosen.data() );
	chosen.resize( n );
	return chosen;
}

// the radius CG_AddSaberBlade gives a full length saber, every frame:
// length * 2 + random() * 8 (cg_players.c)
float SaberRadius( std::mt19937 &rng ) {
	return 40.0f * 2.0f + std::uniform_real_distribution<float>( 0.0f, 1.0f )( rng ) * 8.0f;
}

// players in front of the viewer, 100 to 1500 units away, each with a saber
// light at the middle of its blade (all saber colors have a full component),
// the blades of two players at least 40 units apart
std::vector<dlselLight_t> SaberCrowd( int count, std::mt19937 &rng ) {
	std::uniform_real_distribution<float> unit( 0.0f, 1.0f );
	std::vector<dlselLight_t> crowd;
	while ( (int)crowd.size() < count ) {
		const float d = 100.0f + 1400.0f * unit( rng );
		const float a = ( unit( rng ) - 0.5f ) * 1.4f;
		const dlselLight_t dl = Light( d * std::cos( a ), d * std::sin( a ), 40.0f, 80.0f );
		bool apart = true;
		for ( const dlselLight_t &other : crowd ) {
			const float dx = dl.origin[0] - other.origin[0], dy = dl.origin[1] - other.origin[1];
			apart = apart && dx * dx + dy * dy >= 40.0f * 40.0f;
		}
		if ( apart ) {
			crowd.push_back( dl );
		}
	}
	return crowd;
}

struct Flicker {
	int changedFrames = 0;	// frames whose kept set differs from the previous frame's
	int maxToggles = 0;		// times the most unstable light switched on or off
	int blinks = 0;			// lights switched back within 10 frames (80 ms at 125 fps)
};

// frames of a crowd whose saber radii jitter, players moving at up to speed
// units per frame (0: standing still)
Flicker RunCrowd( int count, unsigned seed, bool withHistory, float speed, int frames = 1000 ) {
	std::mt19937 rng( seed );
	std::uniform_real_distribution<float> unit( -1.0f, 1.0f );
	std::vector<dlselLight_t> crowd = SaberCrowd( count, rng );
	std::vector<std::array<float, 2>> velocity;
	for ( int i = 0; i < count; i++ ) {
		velocity.push_back( { { unit( rng ) * speed, unit( rng ) * speed } } );
	}

	dlselHistory_t history = {};
	std::set<int> previous;
	std::vector<int> toggles( count, 0 ), lastToggle( count, -1000 );
	Flicker result;
	for ( int frame = 0; frame < frames; frame++ ) {
		for ( int i = 0; i < count; i++ ) {
			crowd[i].radius = SaberRadius( rng );
			for ( int j = 0; j < 2; j++ ) {
				crowd[i].origin[j] += velocity[i][j];
			}
			if ( crowd[i].origin[0] < 100.0f || crowd[i].origin[0] > 1500.0f ) {
				velocity[i][0] = -velocity[i][0];
			}
			if ( std::fabs( crowd[i].origin[1] ) > 1000.0f ) {
				velocity[i][1] = -velocity[i][1];
			}
		}
		const std::vector<int> chosen = Select( crowd, withHistory ? &history : nullptr );
		const std::set<int> kept( chosen.begin(), chosen.end() );
		if ( frame > 0 && kept != previous ) {
			result.changedFrames++;
			for ( int i = 0; i < count; i++ ) {
				if ( kept.count( i ) != previous.count( i ) ) {
					toggles[i]++;
					if ( frame - lastToggle[i] <= 10 ) {
						result.blinks++;
					}
					lastToggle[i] = frame;
				}
			}
		}
		previous = kept;
	}
	for ( int t : toggles ) {
		result.maxToggles = std::max( result.maxToggles, t );
	}
	return result;
}

} // namespace

TEST(DlightSelect, KeepsEveryLightUpToTheLimit) {
	std::vector<dlselLight_t> lights;
	for ( int i = 0; i < kMaxDlights; i++ ) {
		lights.push_back( Light( -500.0f - i, 0.0f, 0.0f, 100.0f ) );	// even behind the viewer
	}
	const std::vector<int> chosen = Select( lights );
	ASSERT_EQ( (int)chosen.size(), kMaxDlights );
	for ( int i = 0; i < kMaxDlights; i++ ) {
		EXPECT_EQ( chosen[i], i );
	}
}

TEST(DlightSelect, DropsLightsEntirelyBehindTheViewer) {
	// testscene ... dlights: 40 lights behind the camera, added first, then
	// one in front
	std::vector<dlselLight_t> lights;
	for ( int i = 0; i < 40; i++ ) {
		lights.push_back( Light( -300.0f - 4.0f * i, 0.0f, 0.0f, 100.0f ) );
	}
	lights.push_back( Light( 192.0f, 0.0f, -32.0f, 200.0f ) );
	const std::vector<int> chosen = Select( lights );
	ASSERT_EQ( (int)chosen.size(), kMaxDlights );
	EXPECT_EQ( chosen.back(), 40 );
}

TEST(DlightSelect, KeepsTheBrightestAndClosestInTheirOrder) {
	// 40 lights in front, the closer the brighter; every other one is dim
	std::vector<dlselLight_t> lights;
	for ( int i = 0; i < 40; i++ ) {
		lights.push_back( Light( 100.0f + 50.0f * i, 0.0f, 0.0f, 100.0f, ( i & 1 ) ? 0.1f : 1.0f ) );
	}
	const std::vector<int> chosen = Select( lights );
	ASSERT_EQ( (int)chosen.size(), kMaxDlights );
	for ( size_t i = 1; i < chosen.size(); i++ ) {
		EXPECT_LT( chosen[i - 1], chosen[i] ) << "kept lights must stay in the order they were added";
	}
	// the 8 dropped are the dimmest far ones
	const std::set<int> kept( chosen.begin(), chosen.end() );
	for ( int i : { 25, 27, 29, 31, 33, 35, 37, 39 } ) {
		EXPECT_EQ( kept.count( i ), 0u ) << "light " << i;
	}
}

TEST(DlightSelect, HistoryHoldsTheChoice) {
	std::vector<dlselLight_t> lights;
	for ( int i = 0; i < 20; i++ ) {
		lights.push_back( Light( 100.0f + 10.0f * i, 0.0f, 0.0f, 100.0f ) );
	}
	dlselHistory_t history = {};
	const std::vector<int> chosen = Select( lights, &history );
	EXPECT_EQ( (int)chosen.size(), 20 );
	ASSERT_EQ( history.count, 20 );
	for ( int i = 0; i < 20; i++ ) {
		EXPECT_FLOAT_EQ( history.origins[i][0], lights[i].origin[0] );
	}
}

// The regression: with the cgame's per-frame saber radius jitter, lights
// near the cutoff used to swap in and out every frame while nothing moved.
// With the history it can't happen: the blades are more than 32 units
// apart, so each kept light only matches itself, and for a kept light A
// and a dropped one B, sB / sA <= (88/80)^2 * (88/80)^2 < 1.5.
TEST(DlightSelect, StandingSaberCrowdDoesNotFlicker) {
	int changedWithoutHistory = 0;
	for ( int count : { 33, 36, 40, 48, 64 } ) {
		for ( unsigned seed : { 1u, 2u, 3u } ) {
			const Flicker with = RunCrowd( count, seed, true, 0.0f );
			EXPECT_EQ( with.changedFrames, 0 ) << count << " lights, seed " << seed;
			changedWithoutHistory += RunCrowd( count, seed, false, 0.0f ).changedFrames;
		}
	}
	// the scene does stress the cutoff: ranking on the scores alone (the
	// selection before the history) changes the kept set in a large part of
	// the frames (60% with the MSVC library; the layouts depend on it)
	EXPECT_GT( changedWithoutHistory, 15 * 999 / 10 );
}

TEST(DlightSelect, MovingSaberCrowdOnlyChangesWhenLightsReallyCross) {
	for ( int count : { 40, 64 } ) {
		for ( unsigned seed : { 1u, 2u } ) {
			// running players, 2.5 units per frame at 125 fps
			const Flicker with = RunCrowd( count, seed, true, 2.5f );
			const Flicker without = RunCrowd( count, seed, false, 2.5f );
			// MSVC library layouts: 12 to 31 changed frames, at most 3
			// switches of one light and no blink, against 512 to 807
			// changed frames and 1074 to 2042 blinks without the history
			EXPECT_LT( with.changedFrames * 10, without.changedFrames ) << count << " lights, seed " << seed;
			EXPECT_LE( with.maxToggles, 6 ) << count << " lights, seed " << seed;
			EXPECT_LE( with.blinks, 5 ) << count << " lights, seed " << seed;
			EXPECT_GT( without.blinks, 200 ) << count << " lights, seed " << seed;
		}
	}
}

TEST(DlightSelect, SaberAtTheBehindCullDoesNotBlink) {
	// a player right behind the viewer, his blade about its radius behind
	// the view plane: the jittering radius moves it in and out of the cull
	std::mt19937 rng( 5 );
	std::vector<dlselLight_t> lights;
	for ( int i = 0; i < 32; i++ ) {
		lights.push_back( Light( 600.0f + 20.0f * i, -300.0f + 20.0f * i, 40.0f, 80.0f ) );
	}
	lights.push_back( Light( -84.0f, 100.0f, 40.0f, 80.0f ) );
	dlselHistory_t history = {};
	for ( bool withHistory : { false, true } ) {
		std::set<int> previous;
		int changedFrames = 0;
		for ( int frame = 0; frame < 500; frame++ ) {
			lights[32].radius = SaberRadius( rng );
			const std::vector<int> chosen = Select( lights, withHistory ? &history : nullptr );
			const std::set<int> kept( chosen.begin(), chosen.end() );
			changedFrames += frame > 0 && kept != previous;
			previous = kept;
		}
		if ( withHistory ) {
			EXPECT_LE( changedFrames, 1 );
		} else {
			EXPECT_GT( changedFrames, 100 ) << "the scene doesn't reach the cull (test changed?)";
		}
	}
}

TEST(DlightSelect, BladeToBladePairDoesNotSwap) {
	// 31 close lights and, for the last place, two players 20 units apart
	std::mt19937 rng( 11 );
	std::vector<dlselLight_t> lights;
	for ( int i = 0; i < 31; i++ ) {
		lights.push_back( Light( 150.0f + 10.0f * i, -400.0f, 40.0f, 80.0f ) );
	}
	lights.push_back( Light( 1000.0f, 0.0f, 40.0f, 80.0f ) );
	lights.push_back( Light( 1000.0f, 20.0f, 40.0f, 80.0f ) );
	dlselHistory_t history = {};
	std::set<int> first;
	for ( int frame = 0; frame < 500; frame++ ) {
		lights[31].radius = SaberRadius( rng );
		lights[32].radius = SaberRadius( rng );
		const std::vector<int> chosen = Select( lights, &history );
		const std::set<int> kept( chosen.begin(), chosen.end() );
		if ( frame == 0 ) {
			first = kept;
		}
		ASSERT_EQ( kept, first ) << "frame " << frame;
	}
}

TEST(DlightSelect, TwoKeptLightsAtTheSameSpotBothHoldTheirPlace) {
	// 30 close lights and two at the same spot (a muzzle flash and its
	// effect) fill the 32 places; then a light 1.2 times as bright as each
	// of the two appears elsewhere: not enough to replace one of them
	std::vector<dlselLight_t> lights;
	for ( int i = 0; i < 30; i++ ) {
		lights.push_back( Light( 150.0f + 10.0f * i, -400.0f, 40.0f, 80.0f ) );
	}
	lights.push_back( Light( 1000.0f, 0.0f, 40.0f, 80.0f ) );
	lights.push_back( Light( 1000.0f, 0.0f, 40.0f, 80.0f ) );
	dlselHistory_t history = {};
	EXPECT_EQ( (int)Select( lights, &history ).size(), 32 );
	// 1.25 times as far squared, 1.5 times as bright
	lights.push_back( Light( 1000.0f, 500.0f, 40.0f, 80.0f, 1.5f ) );
	const std::vector<int> chosen = Select( lights, &history );
	const std::set<int> kept( chosen.begin(), chosen.end() );
	EXPECT_EQ( kept.count( 30 ), 1u );
	EXPECT_EQ( kept.count( 31 ), 1u );
	EXPECT_EQ( kept.count( 32 ), 0u );
}

TEST(DlightSelect, MuchBrighterLightStillTakesAPlace) {
	std::mt19937 rng( 7 );
	std::vector<dlselLight_t> crowd = SaberCrowd( 40, rng );
	dlselHistory_t history = {};
	for ( int frame = 0; frame < 10; frame++ ) {
		Select( crowd, &history );
	}
	// an explosion next to the viewer
	crowd.push_back( Light( 150.0f, 20.0f, 0.0f, 300.0f ) );
	const std::vector<int> chosen = Select( crowd, &history );
	ASSERT_EQ( (int)chosen.size(), kMaxDlights );
	EXPECT_EQ( chosen.back(), 40 );
}

TEST(DlightSelect, KeptLightThatMovedAwayLosesItsPlace) {
	// 33 equal lights in a row: the farthest one is dropped; then the light
	// that took the 32nd place jumps 400 units away (a respawn) and loses it
	std::vector<dlselLight_t> lights;
	for ( int i = 0; i < 33; i++ ) {
		lights.push_back( Light( 200.0f + 40.0f * i, 0.0f, 0.0f, 80.0f ) );
	}
	dlselHistory_t history = {};
	std::vector<int> chosen = Select( lights, &history );
	EXPECT_EQ( std::set<int>( chosen.begin(), chosen.end() ).count( 32 ), 0u );
	lights[31].origin[0] += 400.0f;
	chosen = Select( lights, &history );
	const std::set<int> kept( chosen.begin(), chosen.end() );
	EXPECT_EQ( kept.count( 31 ), 0u );
	EXPECT_EQ( kept.count( 32 ), 1u );
}

// With a mirror in the scene the main view no longer culls: the mirror can
// show what is behind the viewer.
TEST(DlightSelect, MirrorKeepsLightsBehindTheViewer) {
	// 32 lights in front, 1000 units and more away, and a close one behind
	std::vector<dlselLight_t> lights;
	for ( int i = 0; i < 32; i++ ) {
		lights.push_back( Light( 1000.0f + 20.0f * i, 0.0f, 0.0f, 100.0f ) );
	}
	lights.push_back( Light( -200.0f, 0.0f, 0.0f, 100.0f ) );
	std::set<int> kept;
	for ( int i : Select( lights ) ) {
		kept.insert( i );
	}
	EXPECT_EQ( kept.count( 32 ), 0u ) << "behind the viewer, out of its radius";
	kept.clear();
	for ( int i : Select( lights, nullptr, std::vector<dlselView_t>( 1, View( false ) ) ) ) {
		kept.insert( i );
	}
	EXPECT_EQ( kept.count( 32 ), 1u ) << "a mirror may show it";
	EXPECT_EQ( kept.count( 31 ), 0u ) << "the farthest light makes room";
}

// A portal shows the area around its camera, maybe far from the viewer.
TEST(DlightSelect, PortalCameraRanksTheLightsNearIt) {
	// 32 lights around the viewer, and one next to a camera 3000 units away
	std::vector<dlselLight_t> lights;
	for ( int i = 0; i < 32; i++ ) {
		lights.push_back( Light( 300.0f + 10.0f * i, 100.0f, 0.0f, 100.0f ) );
	}
	lights.push_back( Light( 3000.0f, 3000.0f, 0.0f, 100.0f ) );
	std::set<int> kept;
	for ( int i : Select( lights ) ) {
		kept.insert( i );
	}
	EXPECT_EQ( kept.count( 32 ), 0u );

	dlselView_t camera = { { 3050.0f, 3000.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, false };
	std::vector<dlselView_t> views( 1, View( false ) );
	views.push_back( camera );
	kept.clear();
	for ( int i : Select( lights, nullptr, views ) ) {
		kept.insert( i );
	}
	EXPECT_EQ( kept.count( 32 ), 1u ) << "next to the portal camera";
	EXPECT_EQ( kept.count( 31 ), 0u ) << "the farthest light from both views makes room";
}
