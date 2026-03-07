// sv_game_entities.h -- Game entity array management
// Extracted from sv_game.cpp as part of aggressive server refactoring

#ifndef SV_GAME_ENTITIES_H
#define SV_GAME_ENTITIES_H

// these functions must be used instead of pointer arithmetic, because
// the game allocates gentities with private information after the server shared part
int	SV_NumForGentity( sharedEntity_t *ent ) {
	int		num;

	num = ( (byte *)ent - (byte *)sv.gentities ) / sv.gentitySize;

	return num;
}

sharedEntity_t *SV_GentityNum( int num ) {
	sharedEntity_t *ent;

	if ( (unsigned)num >= (unsigned)sv.num_entities ) {
		Com_Error( ERR_DROP, "SV_GentityNum: bad num" );
	}

	ent = (sharedEntity_t *)((byte *)sv.gentities + sv.gentitySize*(num));

	return ent;
}

mvsharedEntity_t *MV_EntityNum( int num )
{
	if ( (unsigned)num >= (unsigned)sv.num_entities ) {
		Com_Error( ERR_DROP, "MV_EntityNum: bad num" );
	}

	return (mvsharedEntity_t *)( (byte *)sv.gentitiesMV + (sv.gentitySizeMV*num) );
}

playerState_t *SV_GameClientNum( int num ) {
	playerState_t	*ps;

	ps = (playerState_t *)((byte *)sv.gameClients + sv.gameClientSize*(num));

	return ps;
}

svEntity_t	*SV_SvEntityForGentity( sharedEntity_t *gEnt ) {
	int	num = -1;

	if ( !gEnt ) {
		Com_Error( ERR_DROP, "SV_SvEntityForGentity: null gEnt" );
	}
	if ( mv_fixplayerghosting->integer && !(sv.fixes & MVFIX_PLAYERGHOSTING) ) {
		num = SV_NumForGentity( gEnt );
	}
	// fallback to s.number if gEnt is not an element of gentities array
	if ( (unsigned)num >= (unsigned)MAX_GENTITIES ) {
		num = gEnt->s.number;
	}
	if ( (unsigned)num >= (unsigned)MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "SV_SvEntityForGentity: bad gEnt" );
	}

	return &sv.svEntities[ num ];
}

sharedEntity_t *SV_GEntityForSvEntity( svEntity_t *svEnt ) {
	int		num;

	num = svEnt - sv.svEntities;
	return SV_GentityNum( num );
}

#endif // SV_GAME_ENTITIES_H
