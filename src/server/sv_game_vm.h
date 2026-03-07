// sv_game_vm.h -- Game VM lifecycle management
// Extracted from sv_game.cpp as part of aggressive server refactoring

#ifndef SV_GAME_VM_H
#define SV_GAME_VM_H

/*
===============
SV_ShutdownGameProgs

Called every time a map changes
===============
*/
void SV_ShutdownGameProgs( void ) {
	if ( !gvm ) {
		return;
	}
	VM_Call( gvm, GAME_SHUTDOWN, qfalse );
	VM_Free( gvm );
	gvm = NULL;
	sv.fixes = MVFIX_NONE;
	sv.vmPlayerSnapshots = qfalse;
	sv.submodelBypass = qfalse;
}

/*
==================
SV_InitGameVM

Called for both a full init and a restart
==================
*/
extern void FixGhoul2InfoLeaks(bool);
static void SV_InitGameVM( qboolean restart ) {
	int		i;
	int apireq;

	FixGhoul2InfoLeaks(true);

	// clear physics interaction links
	SV_ClearWorld ();

	// start the entity parsing at the beginning
	sv.entityParsePoint = (const char *) CM_EntityString();

	// use the current msec count for a random seed
	// init for this gamestate

	mvStructConversionDisabled = qfalse;

	apireq = VM_Call(gvm, GAME_INIT, sv.time, Com_Milliseconds(), restart,
		0, 0, 0, 0, 0, 0, 0, 0, MIN(mv_apienabled->integer, MV_APILEVEL));
	if (apireq > mv_apienabled->integer) {
		apireq = mv_apienabled->integer;
	}
	VM_SetMVAPILevel(gvm, apireq);
	Com_DPrintf("GameVM uses MVAPI level %i.\n", apireq);

	if (apireq >= 1) {
		VM_Call(gvm, MVAPI_AFTER_INIT);
	}

	if (!sv.gentities || !sv.gameClients) {
		Com_Error(ERR_DROP, "SV_InitGameVM: failed to locate game data\n");
	}

	// clear all gentity pointers that might still be set from
	// a previous level
	for ( i = 0 ; i < sv_maxclients->integer ; i++ ) {
		svs.clients[i].gentity = NULL;
	}
}

/*
===================
SV_RestartGameProgs

Called on a map_restart, but not on a normal map change
===================
*/
void SV_RestartGameProgs( void ) {
	if ( !gvm ) {
		return;
	}
	VM_Call( gvm, GAME_SHUTDOWN, qtrue );

	// do a restart instead of a free
	gvm = VM_Restart( gvm );
	if ( !gvm ) {
		Com_Error( ERR_FATAL, "VM_Restart on game failed" );
	}

	SV_InitGameVM( qtrue );
}

/*
===============
SV_InitGameProgs

Called on a normal map change, not on a map_restart
===============
*/
void SV_InitGameProgs( void ) {
	cvar_t	*var;
	extern int	bot_enable;

	var = Cvar_Get( "bot_enable", "1", CVAR_LATCH );
	if ( var ) {
		bot_enable = var->integer;
	}
	else {
		bot_enable = 0;
	}

	// load the dll or bytecode
	gvm = VM_Create( "jk2mpgame", qfalse, SV_GameSystemCalls, (vmInterpret_t)(int)Cvar_VariableValue( "vm_game" ) );
	if ( !gvm ) {
		Com_Error( ERR_FATAL, "VM_Create on game failed" );
	}

	SV_InitGameVM( qfalse );
}

/*
====================
SV_GameCommand

See if the current console command is claimed by the game
====================
*/
qboolean SV_GameCommand( void ) {
	if ( sv.state != SS_GAME ) {
		return qfalse;
	}

	return (qboolean)!!VM_Call( gvm, GAME_CONSOLE_COMMAND );
}

/*
====================
SV_MVAPI_ControlFixes

disable / enable toggleable fixes from the gvm
====================
*/
qboolean SV_MVAPI_ControlFixes(int fixes) {
	int mask = 0;

	switch (VM_MVAPILevel(gvm)) {
	case 3:
		mask |= MVFIX_PLAYERGHOSTING;
		// fallthrough
	case 2:
		mask |= MVFIX_SABERSTEALING;
		// fallthrough
	case 1:
		mask |= MVFIX_NAMECRASH;
		mask |= MVFIX_FORCECRASH;
		mask |= MVFIX_GALAKING;
		mask |= MVFIX_BROKENMODEL;
		mask |= MVFIX_TURRETCRASH;
		mask |= MVFIX_CHARGEJUMP;
		mask |= MVFIX_SPEEDHACK;
	}

	sv.fixes = fixes & mask;

	return qfalse;
}

/*
====================
SV_MVAPI_EnablePlayerSnapshots

enable / disable whether to call the gvm before generating each snapshot
====================
*/
qboolean SV_MVAPI_EnablePlayerSnapshots(qboolean enable) {
	sv.vmPlayerSnapshots = enable;
	return qfalse;
}

/*
====================
SV_MVAPI_EnableSubmodelBypass
====================
*/
qboolean SV_MVAPI_EnableSubmodelBypass(qboolean enable) {
	sv.submodelBypass = enable;
	return sv.submodelBypass;
}

#endif // SV_GAME_VM_H
