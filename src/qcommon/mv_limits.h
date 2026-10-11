// mv_limits.h -- the client and entity limits of the protocol in use
#ifndef MV_LIMITS_H
#define MV_LIMITS_H

#include "q_shared.h"
#include "../api/mvapi.h"

// MAX_CLIENTS and MAX_GENTITIES (q_shared.h) are compile-time maximums: they
// size the engine's arrays. The limits below are those of the protocol in use
// and of the game modules made for it. Check a client or entity number that
// comes from the network, a module or a cvar against them, and take the
// ENTITYNUM_NONE and ENTITYNUM_WORLD you exchange with a module, botlib or
// SV_Trace from them. Never use them in the renderer: its world number is a
// refEntity number (REFENTITYNUM_WORLD).
//
// The legacy values are numbers on purpose: protocols 15 and 16, and the
// state before any protocol is chosen, keep them whatever the maximums become.
#define MV_LEGACY_MAX_CLIENTS		32
#define MV_LEGACY_GENTITYNUM_BITS	10
#define MV_LEGACY_MAX_GENTITIES		( 1 << MV_LEGACY_GENTITYNUM_BITS )

typedef struct mvprotocolLimits_s {
	int		maxClients;		// the modules' MAX_CLIENTS
	int		gentityBits;	// bits of an entity number on the wire
	int		maxGEntities;	// the modules' MAX_GENTITIES, always 1 << gentityBits
} mvprotocolLimits_t;

// A row fits the engine's arrays, keeps ENTITYNUM_WORLD and ENTITYNUM_NONE
// clear of the clients, and fits the 64-client masks of the module ABI
// (broadcastClients[2], the four 16-bit mind trick masks). Not checked here:
// snapshotIgnore[32] and snapshotEnforce[32] of the MV API's mvsharedEntity_t
// hold 32 clients only, the fixed layout of API levels 1-4 (MV_APILEVEL 5
// needs a struct of its own); MV_ASSERT_LEGACY_MVAPI_CLIENTS() guards the site
// that indexes them.
#define MV_LIMITS_ROW_VALID( c, b, e )	( (c) >= 1 && (c) <= MAX_CLIENTS && (c) <= 64 \
	&& (b) >= 1 && (e) == ( 1 << (b) ) && (e) <= MAX_GENTITIES && (c) < (e) - 2 )

static_assert( MV_LIMITS_ROW_VALID( MV_LEGACY_MAX_CLIENTS, MV_LEGACY_GENTITYNUM_BITS, MV_LEGACY_MAX_GENTITIES ),
	"the legacy limits must fit the compile-time maximums" );

// Step 1 changes no value. Step 2 removes this when it raises the maximums,
// after making each MV_ASSERT_LEGACY_* site per protocol.
static_assert( MAX_CLIENTS == MV_LEGACY_MAX_CLIENTS && GENTITYNUM_BITS == MV_LEGACY_GENTITYNUM_BITS
	&& MAX_GENTITIES == MV_LEGACY_MAX_GENTITIES, "step 1 keeps 32 clients and 1024 entities" );

// No code: they stop the build at a site that still assumes the protocol 15/16
// encoding or the configstrings of the 1.02-1.04 modules as soon as a maximum
// changes. The client and entity limits have a row in MV_LimitsForProtocol;
// the model, sound and configstring limits do not yet, and these guards are
// all that marks their sites.
#define MV_ASSERT_LEGACY_ENTITY_ENCODING()	static_assert( GENTITYNUM_BITS == MV_LEGACY_GENTITYNUM_BITS \
	&& MAX_GENTITIES == MV_LEGACY_MAX_GENTITIES, "this site uses the 10-bit entity encoding: make it per protocol first" )
// Model and sound indices in 8 bits: entityState_t modelindex, modelindex2
// and loopSound, and the 8-bit event parameters, which carry sound indices too
// (G_Sound, G_AddEvent( ent, EV_GENERAL_SOUND, index )): entityState_t
// eventParm, playerState_t externalEventParm and eventParms[1], and the
// entityState_t weapon of EV_DEBRIS
#define MV_ASSERT_LEGACY_MODEL_ENCODING()	static_assert( MAX_MODELS == 256 && MAX_SOUNDS == 256, \
	"this site sends model and sound indices in 8 bits: make it per protocol first" )
// The layout of bg_public.h (CS_PLAYERS after 256 models and 256 sounds, 32
// players, CS_MAX) and the gameState_t that CG_GETGAMESTATE copies into the
// cgame. The site includes bg_public.h.
#define MV_ASSERT_LEGACY_CONFIGSTRINGS()	static_assert( MAX_CLIENTS == MV_LEGACY_MAX_CLIENTS \
	&& MAX_MODELS == 256 && MAX_SOUNDS == 256 && CS_PLAYERS == 544 && CS_MAX == 1054 \
	&& MAX_CONFIGSTRINGS == 1400 && MAX_GAMESTATE_CHARS == 16000, \
	"this site uses the configstrings of the 1.02-1.04 modules: make it per protocol first" )
// The site indexes snapshotIgnore and snapshotEnforce of mvsharedEntity_t
// (api/mvapi.h) by client number. That struct is the layout of the modules of
// MV API levels 1-4, which keep it for good: snapshotIgnore[32] at 0,
// snapshotEnforce[32] at 32, mvFlags at 64, 68 bytes. More clients need a
// struct of their own for MV_APILEVEL 5 and a site that reads the module's
// level; growing these arrays would move snapshotEnforce and mvFlags under
// every existing module, so the whole layout is pinned. The member sizes are
// written sizeof( ((T *)0)->member ): VS2013 has no sizeof( T::member ).
#define MV_ASSERT_LEGACY_MVAPI_CLIENTS()	static_assert( MAX_CLIENTS == MV_LEGACY_MAX_CLIENTS \
	&& offsetof( mvsharedEntity_t, snapshotIgnore ) == 0 && sizeof( ((mvsharedEntity_t *)0)->snapshotIgnore ) == 32 \
	&& offsetof( mvsharedEntity_t, snapshotEnforce ) == 32 && sizeof( ((mvsharedEntity_t *)0)->snapshotEnforce ) == 32 \
	&& offsetof( mvsharedEntity_t, mvFlags ) == 64 && sizeof( mvsharedEntity_t ) == 68, \
	"this site indexes the MV API's 32-client arrays of levels 1-4 by client number: make it per API level first" )

// Every protocol has a row; PROTOCOL_UNDEF and unknown values get the legacy one
static inline const mvprotocolLimits_t *MV_LimitsForProtocol( int protocol ) {
	static const mvprotocolLimits_t legacy = { MV_LEGACY_MAX_CLIENTS, MV_LEGACY_GENTITYNUM_BITS, MV_LEGACY_MAX_GENTITIES };

	switch ( protocol ) {
	case PROTOCOL15:
	case PROTOCOL16:
	default:
		return &legacy;
	}
}

#ifndef MV_LIMITS_VERIFY_FOLD
// common.cpp: MV_LimitsForProtocol( MV_GetCurrentProtocol() ). Only the
// accessors below call it.
const mvprotocolLimits_t *MV_CurrentLimits( void );

static inline int MV_MaxClients( void ) {
	return MV_CurrentLimits()->maxClients;
}

static inline int MV_MaxGEntities( void ) {
	return MV_CurrentLimits()->maxGEntities;
}
#else
// tests/limits/verify_fold.py only: the accessors become the old constants, so
// the objects of a pure conversion must equal those of the commit before it
#define MV_MaxClients()		( MAX_CLIENTS )
#define MV_MaxGEntities()	( MAX_GENTITIES )
#endif

// the modules' ENTITYNUM_NONE and ENTITYNUM_WORLD
#define MV_ENTITYNUM_NONE	( MV_MaxGEntities() - 1 )
#define MV_ENTITYNUM_WORLD	( MV_MaxGEntities() - 2 )

#endif // MV_LIMITS_H
