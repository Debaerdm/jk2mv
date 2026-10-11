// mv_limits_stub.h - MV_CurrentLimits (qcommon/common.cpp) for the tests that
// link engine sources reading the limits of the protocol in use. Include it
// once per test executable, after the engine headers. The limits are the
// protocol 16 row unless a ScopedLimits swaps in another valid row.
#ifndef MV_LIMITS_STUB_H
#define MV_LIMITS_STUB_H

#include <cstdlib>

#include "qcommon/mv_limits.h"

static const mvprotocolLimits_t	*g_testLimits;		// NULL: the protocol 16 row

const mvprotocolLimits_t *MV_CurrentLimits( void ) {
	return g_testLimits ? g_testLimits : MV_LimitsForProtocol( PROTOCOL16 );
}

// smaller than the legacy row and valid for every engine array: a site that
// follows the runtime limits behaves differently under it
static const mvprotocolLimits_t	kSmallLimits = { 16, 9, 512 };

class ScopedLimits {
public:
	explicit ScopedLimits( const mvprotocolLimits_t &limits ) {
		if ( !MV_LIMITS_ROW_VALID( limits.maxClients, limits.gentityBits, limits.maxGEntities ) ) {
			abort();	// an invalid row would index past the engine's arrays
		}
		g_testLimits = &limits;
	}
	~ScopedLimits() {
		g_testLimits = NULL;
	}
};

#endif // MV_LIMITS_STUB_H
