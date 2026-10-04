// test_sv_snapshot_ring.cpp - size and bookkeeping of the snapshot entity ring
//
// Unlike the older server tests, this uses the real engine header
// (src/server/sv_snapshot_ring.h), which has no engine dependency.

#include <gtest/gtest.h>
#include <random>
#include <vector>
#include "server/sv_snapshot_ring.h"

namespace {

const int kPacketBackup = 32;	// PACKET_BACKUP, frames per client on a dedicated server
const int kListenFrames = 4;	// frames per client on a listen server
const int kMaxClients = 32;		// MAX_CLIENTS

// the size before sv_snapshotEntityBudget
int ClassicSize( int maxClients, int framesPerClient ) {
	return maxClients * framesPerClient * 64;
}

bool IsPowerOfTwo( int n ) {
	return n > 0 && ( n & ( n - 1 ) ) == 0;
}

struct Frame {
	int first;
	int num;
	int tag;
};

} // namespace

TEST(SvSnapshotRingSize, DefaultBudgetIsWhatAClientCanTake) {
	EXPECT_EQ( SNAPSHOT_ENTITY_BUDGET_DEFAULT, 256 );
	// 32 slots: 32 * 32 * 256 entries instead of 32 * 32 * 64
	EXPECT_EQ( SV_SnapshotEntityRingSize( 32, kPacketBackup, SNAPSHOT_ENTITY_BUDGET_DEFAULT ), 1 << 18 );
	EXPECT_EQ( SV_SnapshotEntityRingSize( 8, kPacketBackup, SNAPSHOT_ENTITY_BUDGET_DEFAULT ), 1 << 16 );
}

TEST(SvSnapshotRingSize, MinimumBudgetKeepsTheClassicSizeForPowerOfTwoSlots) {
	for ( int maxClients = 1; maxClients <= kMaxClients; maxClients *= 2 ) {
		EXPECT_EQ( SV_SnapshotEntityRingSize( maxClients, kPacketBackup, SNAPSHOT_ENTITY_BUDGET_MIN ),
			ClassicSize( maxClients, kPacketBackup ) ) << maxClients << " slots";
	}
}

TEST(SvSnapshotRingSize, NeverSmallerThanTheClassicSizeOrTheBudget) {
	const int frames[] = { kListenFrames, kPacketBackup };
	for ( int f = 0; f < 2; f++ ) {
		for ( int maxClients = 1; maxClients <= kMaxClients; maxClients++ ) {
			for ( int budget = SNAPSHOT_ENTITY_BUDGET_MIN; budget <= SNAPSHOT_ENTITY_BUDGET_MAX; budget += 32 ) {
				const int size = SV_SnapshotEntityRingSize( maxClients, frames[f], budget );
				const int wanted = maxClients * frames[f] * budget;
				EXPECT_TRUE( IsPowerOfTwo( size ) ) << size;
				EXPECT_GE( size, ClassicSize( maxClients, frames[f] ) );
				EXPECT_GE( size, wanted );
				EXPECT_GE( size, 2 * MAX_SNAPSHOT_ENTITIES );
				// rounded up to the next power of two, no further
				if ( wanted > 2 * MAX_SNAPSHOT_ENTITIES ) {
					EXPECT_LT( size, 2 * wanted );
				} else {
					EXPECT_EQ( size, 2 * MAX_SNAPSHOT_ENTITIES );
				}
			}
		}
	}
}

TEST(SvSnapshotRingSize, RoundsUpOddSlotCounts) {
	// 24 slots: 24 * 32 * 64 = 49152 entries became 65536
	EXPECT_EQ( SV_SnapshotEntityRingSize( 24, kPacketBackup, 64 ), 1 << 16 );
	EXPECT_EQ( SV_SnapshotEntityRingSize( 17, kPacketBackup, 256 ), 1 << 18 );
	EXPECT_EQ( SV_SnapshotEntityRingSize( 16, kPacketBackup, 256 ), 1 << 17 );
}

TEST(SvSnapshotRingSize, BudgetIsClamped) {
	EXPECT_EQ( SV_ClampSnapshotEntityBudget( -5 ), SNAPSHOT_ENTITY_BUDGET_MIN );
	EXPECT_EQ( SV_ClampSnapshotEntityBudget( 0 ), SNAPSHOT_ENTITY_BUDGET_MIN );
	EXPECT_EQ( SV_ClampSnapshotEntityBudget( 63 ), SNAPSHOT_ENTITY_BUDGET_MIN );
	EXPECT_EQ( SV_ClampSnapshotEntityBudget( 300 ), 300 );
	EXPECT_EQ( SV_ClampSnapshotEntityBudget( 100000 ), SNAPSHOT_ENTITY_BUDGET_MAX );

	EXPECT_EQ( SV_SnapshotEntityRingSize( 32, kPacketBackup, 0 ),
		SV_SnapshotEntityRingSize( 32, kPacketBackup, SNAPSHOT_ENTITY_BUDGET_MIN ) );
	EXPECT_EQ( SV_SnapshotEntityRingSize( 32, kPacketBackup, 0x7FFFFFFF ),
		SV_SnapshotEntityRingSize( 32, kPacketBackup, SNAPSHOT_ENTITY_BUDGET_MAX ) );
	// the largest ring: 32 * 32 * 1024 entries, about 296 MB
	EXPECT_EQ( SV_SnapshotEntityRingSize( kMaxClients, kPacketBackup, SNAPSHOT_ENTITY_BUDGET_MAX ), 1 << 20 );
}

TEST(SvSnapshotRingSize, SmallListenServerHoldsTwoFullSnapshots) {
	// 1 slot * 4 frames * 64 used to be 256 entries, less than one full snapshot
	EXPECT_EQ( SV_SnapshotEntityRingSize( 1, kListenFrames, 64 ), 2 * MAX_SNAPSHOT_ENTITIES );
	EXPECT_EQ( SV_SnapshotEntityRingSize( 0, 0, 64 ), 2 * MAX_SNAPSHOT_ENTITIES );
}

TEST(SvSnapshotRingIndex, MaskMatchesModulo) {
	// the counter is never negative, so the mask finds the same entry as the
	// old modulo for every ring size that already was a power of two
	std::mt19937 rng( 1234 );
	std::uniform_int_distribution<int> counter( 0, SNAPSHOT_ENTITIES_LIMIT );
	for ( int size = 2 * MAX_SNAPSHOT_ENTITIES; size <= ( 1 << 20 ); size <<= 1 ) {
		for ( int i = 0; i < 10000; i++ ) {
			const int n = counter( rng );
			ASSERT_EQ( n & ( size - 1 ), n % size ) << n << " in " << size;
		}
		EXPECT_EQ( SNAPSHOT_ENTITIES_LIMIT & ( size - 1 ), SNAPSHOT_ENTITIES_LIMIT % size );
		EXPECT_EQ( 0 & ( size - 1 ), 0 );
	}
}

TEST(SvSnapshotRingRollOff, NeverKeepsAnOverwrittenFrame) {
	// build snapshots of random sizes into a ring the way SV_BuildClientSnapshot
	// does, starting near the counter limit, and check that every frame the
	// roll-off test still accepts has all its entities intact
	const int sizes[] = { 2 * MAX_SNAPSHOT_ENTITIES, 1 << 13, 1 << 15 };
	const int starts[] = { 0, SNAPSHOT_ENTITIES_LIMIT - 4 * kMaxClients * MAX_SNAPSHOT_ENTITIES - 3000000 };
	std::mt19937 rng( 99 );
	std::uniform_int_distribution<int> entities( 0, MAX_SNAPSHOT_ENTITIES );

	for ( int s = 0; s < 3; s++ ) {
		for ( int st = 0; st < 2; st++ ) {
			const int ringSize = sizes[s];
			std::vector<int> ring( ringSize, -1 );
			std::vector<Frame> frames;
			int next = starts[st];
			int accepted = 0, rejected = 0;

			for ( int tag = 0; tag < 1000; tag++ ) {
				Frame frame = { next, entities( rng ), tag };
				for ( int i = 0; i < frame.num; i++ ) {
					ring[next & ( ringSize - 1 )] = tag;
					next++;
					ASSERT_LT( next, SNAPSHOT_ENTITIES_LIMIT );
				}
				frames.push_back( frame );
				if ( frames.size() > 128 ) {
					frames.erase( frames.begin() );
				}

				for ( size_t f = 0; f < frames.size(); f++ ) {
					const Frame &old = frames[f];
					if ( SV_SnapshotEntitiesRolledOff( old.first, next, ringSize ) ) {
						// its first entity is gone, or is the oldest one kept
						// (the test is conservative by one entry)
						if ( old.num > 0 ) {
							EXPECT_TRUE( ring[old.first & ( ringSize - 1 )] != old.tag || old.first == next - ringSize );
						}
						rejected++;
						continue;
					}
					accepted++;
					for ( int i = 0; i < old.num; i++ ) {
						ASSERT_EQ( ring[( old.first + i ) & ( ringSize - 1 )], old.tag )
							<< "frame " << old.tag << " entity " << i << " ring " << ringSize;
					}
				}
			}
			// both outcomes were exercised
			EXPECT_GT( accepted, 0 );
			EXPECT_GT( rejected, 0 );
		}
	}
}

TEST(SvSnapshotRingRollOff, LargerRingKeepsOlderFrames) {
	// 31 bots and a player seeing 200 entities each build 6400 entries per
	// server frame: how many frames back the player can still delta from
	const int perFrame = 32 * 200;
	const int classic = SV_SnapshotEntityRingSize( 32, kPacketBackup, 64 );
	const int bigger = SV_SnapshotEntityRingSize( 32, kPacketBackup, SNAPSHOT_ENTITY_BUDGET_DEFAULT );
	int classicFrames = 0, biggerFrames = 0;
	const int next = 1000 * perFrame;
	for ( int age = 1; age <= 100; age++ ) {
		const int first = next - age * perFrame;
		if ( !SV_SnapshotEntitiesRolledOff( first, next, classic ) ) {
			classicFrames = age;
		}
		if ( !SV_SnapshotEntitiesRolledOff( first, next, bigger ) ) {
			biggerFrames = age;
		}
	}
	EXPECT_EQ( classicFrames, 10 );		// 250 ms at sv_fps 40
	EXPECT_EQ( biggerFrames, 40 );		// 1 s, longer than the PACKET_BACKUP - 3 frames a client can delta from
	EXPECT_GE( biggerFrames, kPacketBackup - 3 );
}

TEST(SvSnapshotRingWrap, RestartLeavesRoomForAFrameAndTheFinalMessage) {
	for ( int maxClients = 1; maxClients <= kMaxClients; maxClients++ ) {
		// a frame builds one snapshot per client, a dropped client (pure check)
		// one more, and SV_FinalMessage two per client on the restart itself
		const int worstCase = 4 * maxClients * MAX_SNAPSHOT_ENTITIES;
		// the highest counter that doesn't restart yet
		int lo = 0, hi = SNAPSHOT_ENTITIES_LIMIT;
		while ( lo < hi ) {
			const int mid = lo + ( hi - lo + 1 ) / 2;
			if ( SV_SnapshotEntitiesWrapping( mid, maxClients ) ) {
				hi = mid - 1;
			} else {
				lo = mid;
			}
		}
		const int next = lo;
		EXPECT_FALSE( SV_SnapshotEntitiesWrapping( next, maxClients ) );
		EXPECT_TRUE( SV_SnapshotEntitiesWrapping( next + 1, maxClients ) );
		// SV_BuildClientSnapshot's fatal error is never reached
		EXPECT_LT( (long long)next + worstCase, (long long)SNAPSHOT_ENTITIES_LIMIT );
		// and the restart doesn't come much earlier than before
		EXPECT_GT( next, SNAPSHOT_ENTITIES_LIMIT - ( 1 << 20 ) );
	}
}

TEST(SvSnapshotRingWrap, IndependentOfTheRingSize) {
	// the old test used the ring size as its margin, which was smaller than a
	// frame's worth of full snapshots on a listen server (4 * 64 per slot)
	EXPECT_TRUE( SV_SnapshotEntitiesWrapping( SNAPSHOT_ENTITIES_LIMIT - 4 * 8 * MAX_SNAPSHOT_ENTITIES, 8 ) );
	EXPECT_FALSE( SV_SnapshotEntitiesWrapping( SNAPSHOT_ENTITIES_LIMIT - 4 * 8 * MAX_SNAPSHOT_ENTITIES - 1, 8 ) );
	EXPECT_FALSE( SV_SnapshotEntitiesWrapping( 0, kMaxClients ) );
}
