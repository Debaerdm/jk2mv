// test_sv_snapshot_ring.cpp - size and bookkeeping of the snapshot entity ring
//
// Unlike the older server tests, this uses the real engine header
// (src/server/sv_snapshot_ring.h), which has no engine dependency.

#include <gtest/gtest.h>
#include <random>
#include <vector>
#include "server/sv_snapshot_ring.h"

namespace {

const int kPacketBackup = 32;	// PACKET_BACKUP, the frames the engine keeps per slot
const int kMaxClients = 32;		// MAX_CLIENTS
// SV_WriteSnapshotToClient deltas from a frame at most this many messages
// back (outgoingSequence - deltaMessage < PACKET_BACKUP - 3)
const int kOldestDeltaBase = kPacketBackup - 4;

// a dedicated server's ring before sv_snapshotEntityBudget
int ClassicSize( int maxClients ) {
	return maxClients * kPacketBackup * 64;
}

bool IsPowerOfTwo( int n ) {
	return n > 0 && ( n & ( n - 1 ) ) == 0;
}

struct Frame {
	int first;
	int num;
	int tag;
};

// Every slot builds a snapshot of perSnapshot entities each server frame, as
// bots do, into a ring of ringSize the way SV_BuildClientSnapshot does.
// Returns whether the frame each slot built kOldestDeltaBase frames earlier
// is still accepted by the roll-off test, checking that an accepted one
// really is intact.
bool OldestDeltaBaseKept( int maxClients, int ringSize, int perSnapshot ) {
	std::vector<int> ring( ringSize, -1 );
	std::vector<int> first( maxClients * kPacketBackup, 0 );	// client->frames[].first_entity
	int next = 1000003;
	bool kept = true;

	for ( int frame = 0; frame < kPacketBackup + 4; frame++ ) {
		for ( int c = 0; c < maxClients; c++ ) {
			first[c * kPacketBackup + frame % kPacketBackup] = next;
			for ( int i = 0; i < perSnapshot; i++ ) {
				ring[SV_SnapshotEntityIndex( next, ringSize )] = frame * kMaxClients + c;
				next++;
			}
			if ( frame < kOldestDeltaBase ) {
				continue;
			}
			const int baseFrame = frame - kOldestDeltaBase;
			const int base = first[c * kPacketBackup + baseFrame % kPacketBackup];
			if ( SV_SnapshotEntitiesRolledOff( base, next, ringSize ) ) {
				kept = false;
				continue;
			}
			for ( int i = 0; i < perSnapshot; i++ ) {
				EXPECT_EQ( ring[SV_SnapshotEntityIndex( base + i, ringSize )], baseFrame * kMaxClients + c )
					<< "overwritten base accepted: " << maxClients << " slots, entity " << i;
			}
		}
	}
	return kept;
}

} // namespace

TEST(SvSnapshotRingSize, DefaultBudget) {
	EXPECT_GE( SNAPSHOT_ENTITY_BUDGET_DEFAULT, SNAPSHOT_ENTITY_BUDGET_MIN );
	EXPECT_LE( SNAPSHOT_ENTITY_BUDGET_DEFAULT, SNAPSHOT_ENTITY_BUDGET_MAX );
	EXPECT_EQ( SV_ClampSnapshotEntityBudget( SNAPSHOT_ENTITY_BUDGET_DEFAULT ), SNAPSHOT_ENTITY_BUDGET_DEFAULT );
	// twice the classic ring: 32 slots 2^17 entries (37 MB), 8 slots 2^15
	EXPECT_EQ( SV_SnapshotEntityRingSize( 32, kPacketBackup, SNAPSHOT_ENTITY_BUDGET_DEFAULT ), 2 * ClassicSize( 32 ) );
	EXPECT_EQ( SV_SnapshotEntityRingSize( 8, kPacketBackup, SNAPSHOT_ENTITY_BUDGET_DEFAULT ), 2 * ClassicSize( 8 ) );
}

TEST(SvSnapshotRingSize, MinimumBudgetIsTheClassicRing) {
	for ( int maxClients = 1; maxClients <= kMaxClients; maxClients *= 2 ) {
		EXPECT_EQ( SV_SnapshotEntityRingSize( maxClients, kPacketBackup, SNAPSHOT_ENTITY_BUDGET_MIN ),
			ClassicSize( maxClients ) ) << maxClients << " slots";
	}
	// other slot counts round up: 24 slots used to get 49152 entries
	EXPECT_EQ( SV_SnapshotEntityRingSize( 24, kPacketBackup, SNAPSHOT_ENTITY_BUDGET_MIN ), 1 << 16 );
	EXPECT_EQ( SV_SnapshotEntityRingSize( 17, kPacketBackup, 256 ), 1 << 18 );
}

TEST(SvSnapshotRingSize, PowerOfTwoHoldingEveryFrameOfEverySlot) {
	for ( int maxClients = 1; maxClients <= kMaxClients; maxClients++ ) {
		for ( int budget = SNAPSHOT_ENTITY_BUDGET_MIN; budget <= SNAPSHOT_ENTITY_BUDGET_MAX; budget += 25 ) {
			const int size = SV_SnapshotEntityRingSize( maxClients, kPacketBackup, budget );
			const int wanted = maxClients * kPacketBackup * budget;
			EXPECT_TRUE( IsPowerOfTwo( size ) ) << size;
			EXPECT_GE( size, wanted ) << maxClients << " slots, budget " << budget;
			EXPECT_GE( size, ClassicSize( maxClients ) );
			EXPECT_GE( size, SNAPSHOT_ENTITY_RING_MIN );
			// rounded up to the next power of two, no further
			if ( wanted > SNAPSHOT_ENTITY_RING_MIN ) {
				EXPECT_LT( size, 2 * wanted ) << maxClients << " slots, budget " << budget;
			} else {
				EXPECT_EQ( size, SNAPSHOT_ENTITY_RING_MIN );
			}
		}
	}
}

TEST(SvSnapshotRingSize, BudgetIsClamped) {
	EXPECT_EQ( SV_ClampSnapshotEntityBudget( -5 ), SNAPSHOT_ENTITY_BUDGET_MIN );
	EXPECT_EQ( SV_ClampSnapshotEntityBudget( 0 ), SNAPSHOT_ENTITY_BUDGET_MIN );
	EXPECT_EQ( SV_ClampSnapshotEntityBudget( 63 ), SNAPSHOT_ENTITY_BUDGET_MIN );
	EXPECT_EQ( SV_ClampSnapshotEntityBudget( 300 ), 300 );
	EXPECT_EQ( SV_ClampSnapshotEntityBudget( 100000 ), SNAPSHOT_ENTITY_BUDGET_MAX );
	EXPECT_EQ( SV_ClampSnapshotEntityBudget( -2147483647 - 1 ), SNAPSHOT_ENTITY_BUDGET_MIN );

	EXPECT_EQ( SV_SnapshotEntityRingSize( 32, kPacketBackup, 0 ),
		SV_SnapshotEntityRingSize( 32, kPacketBackup, SNAPSHOT_ENTITY_BUDGET_MIN ) );
	EXPECT_EQ( SV_SnapshotEntityRingSize( 32, kPacketBackup, 0x7FFFFFFF ),
		SV_SnapshotEntityRingSize( 32, kPacketBackup, SNAPSHOT_ENTITY_BUDGET_MAX ) );
	// the largest ring: 32 * 32 * 1024 entries, 296 MB
	EXPECT_EQ( SV_SnapshotEntityRingSize( kMaxClients, kPacketBackup, SNAPSHOT_ENTITY_BUDGET_MAX ), 1 << 20 );
}

TEST(SvSnapshotRingSize, NeverBelowTwoFullSnapshots) {
	// a snapshot can't overwrite its own entities or those of the snapshot
	// built just before it, whatever the arguments
	EXPECT_EQ( SNAPSHOT_ENTITY_RING_MIN, 2 * MAX_SNAPSHOT_ENTITIES );
	EXPECT_EQ( SV_SnapshotEntityRingSize( 1, kPacketBackup, SNAPSHOT_ENTITY_BUDGET_MIN ), SNAPSHOT_ENTITY_RING_MIN );
	EXPECT_EQ( SV_SnapshotEntityRingSize( 1, 4, SNAPSHOT_ENTITY_BUDGET_MIN ), SNAPSHOT_ENTITY_RING_MIN );
	EXPECT_EQ( SV_SnapshotEntityRingSize( 0, 0, 0 ), SNAPSHOT_ENTITY_RING_MIN );
}

TEST(SvSnapshotRingIndex, SameSlotAsTheOldModulo) {
	// the counter is never negative, so on every ring the sizing can produce
	// the mask finds the slot the old modulo found
	std::mt19937 rng( 1234 );
	std::uniform_int_distribution<int> counter( 0, SNAPSHOT_ENTITIES_LIMIT );
	for ( int maxClients = 1; maxClients <= kMaxClients; maxClients++ ) {
		for ( int budget = SNAPSHOT_ENTITY_BUDGET_MIN; budget <= SNAPSHOT_ENTITY_BUDGET_MAX; budget *= 2 ) {
			const int size = SV_SnapshotEntityRingSize( maxClients, kPacketBackup, budget );
			for ( int i = 0; i < 200; i++ ) {
				const int n = counter( rng );
				ASSERT_EQ( SV_SnapshotEntityIndex( n, size ), n % size ) << n << " in " << size;
			}
			EXPECT_EQ( SV_SnapshotEntityIndex( 0, size ), 0 );
			EXPECT_EQ( SV_SnapshotEntityIndex( size - 1, size ), size - 1 );
			EXPECT_EQ( SV_SnapshotEntityIndex( size, size ), 0 );
			EXPECT_EQ( SV_SnapshotEntityIndex( SNAPSHOT_ENTITIES_LIMIT, size ), SNAPSHOT_ENTITIES_LIMIT % size );
		}
	}
}

TEST(SvSnapshotRingRollOff, NeverKeepsAnOverwrittenFrame) {
	// build snapshots of random sizes into a ring the way SV_BuildClientSnapshot
	// does, from 0 and from just below the restart threshold, and check that
	// every frame the roll-off test still accepts has all its entities intact
	const int sizes[] = { SNAPSHOT_ENTITY_RING_MIN, 1 << 13, 1 << 15 };
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
					ring[SV_SnapshotEntityIndex( next, ringSize )] = tag;
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
							EXPECT_TRUE( ring[SV_SnapshotEntityIndex( old.first, ringSize )] != old.tag || old.first == next - ringSize );
						}
						rejected++;
						continue;
					}
					accepted++;
					for ( int i = 0; i < old.num; i++ ) {
						ASSERT_EQ( ring[SV_SnapshotEntityIndex( old.first + i, ringSize )], old.tag )
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

TEST(SvSnapshotRingRollOff, BudgetIsTheAverageThatKeepsTheWholeDeltaWindow) {
	// a client deltas from at most kOldestDeltaBase frames back: as long as
	// snapshots average no more entities than the budget, that frame is still
	// in the ring when every slot builds a snapshot every server frame
	const int budgets[] = { SNAPSHOT_ENTITY_BUDGET_MIN, SNAPSHOT_ENTITY_BUDGET_DEFAULT, 256 };
	for ( int b = 0; b < 3; b++ ) {
		for ( int maxClients = 1; maxClients <= kMaxClients; maxClients++ ) {
			const int ringSize = SV_SnapshotEntityRingSize( maxClients, kPacketBackup, budgets[b] );
			EXPECT_TRUE( OldestDeltaBaseKept( maxClients, ringSize, budgets[b] ) )
				<< maxClients << " slots, budget " << budgets[b];
		}
		// with no rounding slack, a quarter more entities than the budget
		// loses it
		EXPECT_FALSE( OldestDeltaBaseKept( 32, SV_SnapshotEntityRingSize( 32, kPacketBackup, budgets[b] ), budgets[b] * 5 / 4 ) )
			<< "budget " << budgets[b];
	}
	// so with 32 slots averaging 100 entities, the classic ring loses the
	// oldest delta bases and the default ring keeps them
	EXPECT_FALSE( OldestDeltaBaseKept( 32, ClassicSize( 32 ), 100 ) );
	EXPECT_TRUE( OldestDeltaBaseKept( 32, SV_SnapshotEntityRingSize( 32, kPacketBackup, SNAPSHOT_ENTITY_BUDGET_DEFAULT ), 100 ) );
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
		// and the restart doesn't come much earlier than before, whatever the
		// ring size (the old margin was the ring, up to 2^20 entries now)
		EXPECT_GT( next, SNAPSHOT_ENTITIES_LIMIT - ( 1 << 20 ) );
	}
	EXPECT_FALSE( SV_SnapshotEntitiesWrapping( 0, kMaxClients ) );
}
