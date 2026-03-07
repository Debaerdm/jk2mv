// sv_ccmds_registration.h -- Console command registration
// Extracted from sv_ccmds.cpp as part of aggressive server refactoring

#ifndef SV_CCMDS_REGISTRATION_H
#define SV_CCMDS_REGISTRATION_H

/*
==================
SV_CompleteMapName
==================
*/
static void SV_CompleteMapName( char *args, int argNum ) { // for auto-complete (copied from OpenJK)
	if ( argNum == 2 )
		Field_CompleteFilename( "maps", ".bsp", qtrue );
}

/*
==================
SV_AddOperatorCommands
==================
*/
void SV_AddOperatorCommands( void ) {
	static qboolean	initialized;

	if ( initialized ) {
		return;
	}
	initialized = qtrue;

	Cmd_AddCommand ("heartbeat", SV_Heartbeat_f);
	Cmd_AddCommand ("kick", SV_Kick_f);
	Cmd_AddCommand ("clientkick", SV_KickNum_f);
	Cmd_AddCommand ("status", SV_Status_f);
	Cmd_AddCommand ("serverinfo", SV_Serverinfo_f);
	Cmd_AddCommand ("systeminfo", SV_Systeminfo_f);
	Cmd_AddCommand ("dumpuser", SV_DumpUser_f);
	Cmd_AddCommand ("map_restart", SV_MapRestart_f);
	Cmd_AddCommand ("sectorlist", SV_SectorList_f);
	Cmd_AddCommand ("map", SV_Map_f);
	Cmd_SetCommandCompletionFunc( "map", SV_CompleteMapName );
	Cmd_AddCommand ("devmap", SV_Map_f);
	Cmd_SetCommandCompletionFunc( "devmap", SV_CompleteMapName );
	Cmd_AddCommand ("spmap", SV_Map_f);
	Cmd_AddCommand ("spdevmap", SV_Map_f);
//	Cmd_AddCommand ("devmapbsp", SV_Map_f);	// not used in MP codebase, no server BSP_cacheing
	Cmd_AddCommand ("devmapmdl", SV_Map_f);
	Cmd_SetCommandCompletionFunc( "devmapmdl", SV_CompleteMapName );
	Cmd_AddCommand ("devmapall", SV_Map_f);
	Cmd_SetCommandCompletionFunc( "devmapall", SV_CompleteMapName );
	Cmd_AddCommand ("killserver", SV_KillServer_f);
	Cmd_AddCommand ("svsay", SV_ConSay_f);
	Cmd_AddCommand ("forcetoggle", SV_ForceToggle_f);

	Cmd_AddCommand("whitelistip", SV_WhitelistIP_f);
}

/*
==================
SV_RemoveOperatorCommands
==================
*/
void SV_RemoveOperatorCommands( void ) {
#if 0
	// removing these won't let the server start again
	Cmd_RemoveCommand ("heartbeat");
	Cmd_RemoveCommand ("kick");
	Cmd_RemoveCommand ("banUser");
	Cmd_RemoveCommand ("banClient");
	Cmd_RemoveCommand ("status");
	Cmd_RemoveCommand ("serverinfo");
	Cmd_RemoveCommand ("systeminfo");
	Cmd_RemoveCommand ("dumpuser");
	Cmd_RemoveCommand ("map_restart");
	Cmd_RemoveCommand ("sectorlist");
	Cmd_RemoveCommand ("svsay");
#endif
}

#endif // SV_CCMDS_REGISTRATION_H
