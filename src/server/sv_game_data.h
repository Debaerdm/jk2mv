// sv_game_data.h -- Game data location and setup
// Extracted from sv_game.cpp as part of aggressive server refactoring

#ifndef SV_GAME_DATA_H
#define SV_GAME_DATA_H

/*
===============
SV_LocateGameData
===============
*/
void SV_LocateGameData( sharedEntity_t *gEnts, int numGEntities, int sizeofGEntity_t,
					   void *clients, int sizeofGameClient ) {
	if ( gEnts && ( sizeofGEntity_t < (int)sizeof(sharedEntity_t) || numGEntities < 0 ) ) {
		Com_Error( ERR_DROP, "SV_LocateGameData: incorrect game entity data" );
	}

	if (VM_GetGameversion(gvm) == VERSION_1_02) {
		if ( clients && sizeofGameClient < (int)sizeof(playerState15_t) ) {
			Com_Error( ERR_DROP, "SV_LocateGameData: incorrect player state data" );
		}
	} else {
		if ( clients && sizeofGameClient < (int)sizeof(playerState_t) ) {
			Com_Error( ERR_DROP, "SV_LocateGameData: incorrect player state data" );
		}
	}

	sv.gentities = gEnts;
	sv.gentitySize = sizeofGEntity_t;
	sv.num_entities = numGEntities;

	sv.gameClients = clients;
	sv.gameClientSize = sizeofGameClient;
}

/*
===============
MVAPI_LocateGameData
===============
*/
qboolean MVAPI_LocateGameData(mvsharedEntity_t *mvEnts, int numGEntities, int sizeofmvsharedEntity_t) {
	if ( mvEnts && ( numGEntities <= 0 || sizeofmvsharedEntity_t <= 0 ) ) {
		Com_Error( ERR_DROP, "MVAPI_LocateGameData: incorrect shared game entity data" );
	}

	sv.gentitiesMV = mvEnts;
	sv.gentitySizeMV = sizeofmvsharedEntity_t;
	sv.num_entities = numGEntities;

	return qfalse;
}

/*
====================
SV_MVAPI_ResetServerTime

Reset server time on map change
====================
*/
static qboolean SV_MVAPI_ResetServerTime(qboolean enable) {
	sv.resetServerTime = enable ? 1 : 2;
	return qfalse;
}

#endif // SV_GAME_DATA_H
