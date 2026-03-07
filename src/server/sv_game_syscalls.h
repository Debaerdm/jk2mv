// sv_game_syscalls.h -- Game VM syscall dispatcher
// Extracted from sv_game.cpp as part of aggressive server refactoring
// ~800 lines of tightly coupled syscall handling

#ifndef SV_GAME_SYSCALLS_H
#define SV_GAME_SYSCALLS_H

/*
====================
SV_GameSystemCalls

The module is making a system call
====================
*/
extern bool RicksCrazyOnServer;
intptr_t SV_GameSystemCalls( intptr_t *args ) {
	// fix syscalls from 1.02 to match 1.04
	if (VM_GetGameversion(gvm) == VERSION_1_02) {
		if (args[0] > G_G2_GETBOLT_NOREC && args[0] <= G_G2_COLLISIONDETECT) {
			args[0]++;
		}
	}

	// set game ghoul2 context
	RicksCrazyOnServer = true;

	switch( args[0] ) {
	case G_PRINT:
		Com_Printf( "%s", VMAS(1) );
		return 0;
	case G_ERROR:
		Com_Error( ERR_DROP, "%s", VMAS(1) );
		return 0;
	case G_MILLISECONDS:
		return Sys_Milliseconds();
	case G_CVAR_REGISTER:
		Cvar_Register( VMAV(1, vmCvar_t), VMAS(2), VMAS(3), args[4] );
		return 0;
	case G_CVAR_UPDATE:
		Cvar_Update( VMAV(1, vmCvar_t) );
		return 0;
	case G_CVAR_SET:
		Cvar_Set2( VMAS(1), VMAS(2), qtrue, qtrue );
		return 0;
	case G_CVAR_VARIABLE_INTEGER_VALUE:
		return Cvar_VariableIntegerValue( VMAS(1), qtrue );
	case G_CVAR_VARIABLE_STRING_BUFFER:
		Cvar_VariableStringBuffer( VMAS(1), VMAP(2, char, args[3]), args[3], qtrue );
		return 0;
	case G_ARGC:
		return Cmd_Argc();
	case G_ARGV:
		Cmd_ArgvBuffer( args[1], VMAP(2, char, args[3]), args[3] );
		return 0;
	case G_SEND_CONSOLE_COMMAND:
		Cbuf_ExecuteText( (cbufExec_t)args[1], VMAS(2) );
		return 0;

	case G_FS_FOPEN_FILE:
		return FS_FOpenFileByMode( VMAS(1), VMAV(2, int), (fsMode_t)args[3], MODULE_GAME );
	case G_FS_READ:
		FS_Read2( VMAP(1, char, args[2]), args[2], args[3], MODULE_GAME );
		return 0;
	case G_FS_WRITE:
		FS_Write( VMAP(1, char, args[2]), args[2], args[3], MODULE_GAME );
		return 0;
	case G_FS_FCLOSE_FILE:
		FS_FCloseFile( args[1], MODULE_GAME );
		return 0;
	case G_FS_GETFILELIST:
		return FS_GetFileList( VMAS(1), VMAS(2), VMAP(3, char, args[4]), args[4] );

	case G_LOCATE_GAME_DATA:
		SV_LocateGameData( (sharedEntity_t *)VM_ArgArray(args[0], args[1], args[3], args[2]), args[2], args[3], (playerState_t *)VM_ArgArray(args[0], args[4], args[5], MAX_CLIENTS), args[5] );
		VM_LocateGameDataCheck( sv.gentitiesMV, sv.gentitySizeMV, sv.num_entities );
		return 0;
	case G_DROP_CLIENT:
		SV_GameDropClient( args[1], VMAS(2) );
		return 0;
	case G_SEND_SERVER_COMMAND:
		SV_GameSendServerCommand( args[1], VMAS(2) );
		return 0;
	case G_LINKENTITY:
		SV_LinkEntity( VMAIV(1, sharedEntity_t, MAX((int)sizeof(sharedEntity_t), sv.gentitySize)) );
		return 0;
	case G_UNLINKENTITY:
		SV_UnlinkEntity( VMAV(1, sharedEntity_t) );
		return 0;
	case G_ENTITIES_IN_BOX:
		return SV_AreaEntities( VMAP(1, const vec_t, 3), VMAP(2, const vec_t, 3), VMAA(3, int, args[4]), args[4] );
	case G_ENTITY_CONTACT:
		return SV_EntityContact( VMAP(1, const vec_t, 3), VMAP(2, const vec_t, 3), VMAV(3, const sharedEntity_t), qfalse );
	case G_ENTITY_CONTACTCAPSULE:
		return SV_EntityContact( VMAP(1, const vec_t, 3), VMAP(2, const vec_t, 3), VMAV(3, const sharedEntity_t), qtrue );
	case G_TRACE:
		SV_Trace( VMAV(1, trace_t), VMAP(2, const vec_t, 3), VMAP(3, const vec_t, 3), VMAP(4, const vec_t, 3), VMAP(5, const vec_t, 3), args[6], args[7], qfalse, args[8], args[9] );
		return 0;
	case G_TRACECAPSULE:
		SV_Trace( VMAV(1, trace_t), VMAP(2, const vec_t, 3), VMAP(3, const vec_t, 3), VMAP(4, const vec_t, 3), VMAP(5, const vec_t, 3), args[6], args[7], qtrue, args[8], args[9]  );
		return 0;
	case G_POINT_CONTENTS:
		return SV_PointContents( VMAP(1, const vec_t, 3), args[2] );
	case G_SET_BRUSH_MODEL:
		SV_SetBrushModel( VMAV(1, sharedEntity_t), VMAS(2) );
		return 0;
	case G_IN_PVS:
		return SV_inPVS( VMAP(1, const vec_t, 3), VMAP(2, const vec_t, 3) );
	case G_IN_PVS_IGNORE_PORTALS:
		return SV_inPVSIgnorePortals( VMAP(1, const vec_t, 3), VMAP(2, const vec_t, 3) );
	case G_SET_CONFIGSTRING:
		SV_SetConfigstring( args[1], VMAS(2) );
		return 0;
	case G_GET_CONFIGSTRING:
		SV_GetConfigstring( args[1], VMAP(2, char, args[3]), args[3] );
		return 0;
	case G_SET_USERINFO:
		SV_SetUserinfo( args[1], VMAS(2) );
		return 0;
	case G_GET_USERINFO:
		SV_GetUserinfo( args[1], VMAP(2, char, args[3]), args[3] );
		return 0;
	case G_GET_SERVERINFO:
		SV_GetServerinfo( VMAP(1, char, args[2]), args[2] );
		return 0;
	case G_ADJUST_AREA_PORTAL_STATE:
		SV_AdjustAreaPortalState( VMAV(1, sharedEntity_t), (qboolean)!!args[2] );
		return 0;
	case G_AREAS_CONNECTED:
		return CM_AreasConnected( args[1], args[2] );

	case G_BOT_ALLOCATE_CLIENT:
		return SV_BotAllocateClient();
	case G_BOT_FREE_CLIENT:
		SV_BotFreeClient( args[1] );
		return 0;

	case G_GET_USERCMD:
		SV_GetUsercmd( args[1], VMAV(2, usercmd_t) );
		return 0;
	case G_GET_ENTITY_TOKEN:
		if ( CM_NumInlineModels() > MAX_SUBMODELS && !sv.submodelBypass ) {
			Com_Error( ERR_DROP, "MAX_SUBMODELS exceeded (game module doesn't support submodel bypass)" );
		}
		else
		{
			const char	*s;

			s = COM_Parse( &sv.entityParsePoint );
			Q_strncpyz( VMAP(1, char, args[2]), s, args[2] );
			if ( !sv.entityParsePoint && !s[0] ) {
				return qfalse;
			} else {
				return qtrue;
			}
		}

	case G_DEBUG_POLYGON_CREATE:
		return BotImport_DebugPolygonCreate( args[1], args[2], VMAA(3, const vec3_t, args[2]) );
	case G_DEBUG_POLYGON_DELETE:
		BotImport_DebugPolygonDelete( args[1] );
		return 0;
	case G_REAL_TIME:
		return Com_RealTime( VMAV(1, qtime_t) );
	case G_SNAPVECTOR:
		Sys_SnapVector( VMAP(1, vec_t, 3) );
		return 0;

	case SP_REGISTER_SERVER_CMD:
		return SP_RegisterServer( VMAS(1) );
	case SP_GETSTRINGTEXTSTRING:
		return SP_VMGetStringText( VMAS(1), VMAP(2, char, args[3]), args[3] );

	case G_ROFF_CLEAN:
		return theROFFSystem.Clean(qfalse);

	case G_ROFF_UPDATE_ENTITIES:
		theROFFSystem.UpdateEntities(qfalse);
		return 0;

	case G_ROFF_CACHE:
		return theROFFSystem.Cache( VMAS(1), qfalse );

	case G_ROFF_PLAY:
		return theROFFSystem.Play(args[1], args[2], (qboolean)!!args[3], qfalse );

	case G_ROFF_PURGE_ENT:
		return theROFFSystem.PurgeEnt( args[1], qfalse );

		//====================================

	case BOTLIB_SETUP:
		return SV_BotLibSetup();
	case BOTLIB_SHUTDOWN:
		return SV_BotLibShutdown();
	case BOTLIB_LIBVAR_SET:
		return botlib_export->BotLibVarSet( VMAS(1), VMAS(2) );
	case BOTLIB_LIBVAR_GET:
		return botlib_export->BotLibVarGet( VMAS(1), VMAP(2, char, args[3]), args[3] );

	case BOTLIB_PC_ADD_GLOBAL_DEFINE:
		return botlib_export->PC_AddGlobalDefine( VMAS(1) );
	case BOTLIB_PC_LOAD_SOURCE:
		return botlib_export->PC_LoadSourceHandle( VMAS(1) );
	case BOTLIB_PC_FREE_SOURCE:
		return botlib_export->PC_FreeSourceHandle( args[1] );
	case BOTLIB_PC_READ_TOKEN:
		return botlib_export->PC_ReadTokenHandle( args[1], VMAV(2, pc_token_t) );
	case BOTLIB_PC_SOURCE_FILE_AND_LINE:
		return botlib_export->PC_SourceFileAndLine( args[1], VMAP(2, char, MAX_QPATH), VMAV(3, int) );

	case BOTLIB_START_FRAME:
		return botlib_export->BotLibStartFrame( VMF(1) );
	case BOTLIB_LOAD_MAP:
		return botlib_export->BotLibLoadMap( VMAS(1) );
	case BOTLIB_UPDATENTITY:
		return botlib_export->BotLibUpdateEntity( args[1], VMAV(2, const bot_entitystate_t) );
	case BOTLIB_TEST:
		return botlib_export->Test( args[1], NULL, VMAP(3, vec_t, 3), VMAP(4, vec_t, 3) );

	case BOTLIB_GET_SNAPSHOT_ENTITY:
		return SV_BotGetSnapshotEntity( args[1], args[2] );
	case BOTLIB_GET_CONSOLE_MESSAGE:
		return SV_BotGetConsoleMessage( args[1], VMAP(2, char, args[3]), args[3] );
	case BOTLIB_USER_COMMAND:
		SV_ClientThink( args[1], VMAV(2, const usercmd_t) );
		return 0;

	case BOTLIB_AAS_BBOX_AREAS:
		return botlib_export->aas.AAS_BBoxAreas( VMAP(1, const vec_t, 3), VMAP(2, const vec_t, 3), VMAA(3, int, args[4]), args[4] );
	case BOTLIB_AAS_AREA_INFO:
		return botlib_export->aas.AAS_AreaInfo( args[1], VMAIV(2, struct aas_areainfo_s, aas_areainfo_size) );
	case BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL:
		return botlib_export->aas.AAS_AlternativeRouteGoals( VMAP(1, const vec_t, 3), args[2], VMAP(3, const vec_t, 3), args[4], args[5], (struct aas_altroutegoal_s *)VM_ArgArray(args[0], args[6], aas_altroutegoal_size, args[7]), args[7], args[8] );
	case BOTLIB_AAS_ENTITY_INFO:
		botlib_export->aas.AAS_EntityInfo( args[1], VMAIV(2, struct aas_entityinfo_s, aas_entityinfo_size) );
		return 0;

	case BOTLIB_AAS_INITIALIZED:
		return botlib_export->aas.AAS_Initialized();
	case BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX:
		botlib_export->aas.AAS_PresenceTypeBoundingBox( args[1], VMAP(2, vec_t, 3), VMAP(3, vec_t, 3) );
		return 0;
	case BOTLIB_AAS_TIME:
		return FloatAsInt( botlib_export->aas.AAS_Time() );

	case BOTLIB_AAS_POINT_AREA_NUM:
		return botlib_export->aas.AAS_PointAreaNum( VMAP(1, const vec_t, 3) );
	case BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX:
		return botlib_export->aas.AAS_PointReachabilityAreaIndex( VMAP(1, const vec_t, 3) );
	case BOTLIB_AAS_TRACE_AREAS:
		return botlib_export->aas.AAS_TraceAreas( VMAP(1, const vec_t, 3), VMAP(2, const vec_t, 3), VMAA(3, int, args[5]), VMAA(4, vec3_t, args[5]), args[5] );

	case BOTLIB_AAS_POINT_CONTENTS:
		return botlib_export->aas.AAS_PointContents( VMAP(1, const vec_t, 3) );
	case BOTLIB_AAS_NEXT_BSP_ENTITY:
		return botlib_export->aas.AAS_NextBSPEntity( args[1] );
	case BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY:
		return botlib_export->aas.AAS_ValueForBSPEpairKey( args[1], VMAS(2), VMAP(3, char, args[4]), args[4] );
	case BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY:
		return botlib_export->aas.AAS_VectorForBSPEpairKey( args[1], VMAS(2), VMAP(3, vec_t, 3) );
	case BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY:
		return botlib_export->aas.AAS_FloatForBSPEpairKey( args[1], VMAS(2), VMAV(3, float) );
	case BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY:
		return botlib_export->aas.AAS_IntForBSPEpairKey( args[1], VMAS(2), VMAV(3, int) );

	case BOTLIB_AAS_AREA_REACHABILITY:
		return botlib_export->aas.AAS_AreaReachability( args[1] );

	case BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA:
		return botlib_export->aas.AAS_AreaTravelTimeToGoalArea( args[1], VMAP(2, const vec_t, 3), args[3], args[4] );
	case BOTLIB_AAS_ENABLE_ROUTING_AREA:
		return botlib_export->aas.AAS_EnableRoutingArea( args[1], args[2] );
	case BOTLIB_AAS_PREDICT_ROUTE:
		return botlib_export->aas.AAS_PredictRoute( VMAIV(1, struct aas_predictroute_s, aas_predictroute_size), args[2], VMAP(3, const vec_t, 3), args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11] );

	case BOTLIB_AAS_SWIMMING:
		return botlib_export->aas.AAS_Swimming( VMAP(1, const vec_t, 3) );
	case BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT:
		return botlib_export->aas.AAS_PredictClientMovement( VMAIV(1, struct aas_clientmove_s, aas_clientmove_size), args[2], VMAP(3, const vec_t, 3), args[4], args[5],
			VMAP(6, const vec_t, 3), VMAP(7, const vec_t, 3), args[8], args[9], VMF(10), args[11], args[12], args[13] );

	case BOTLIB_EA_SAY:
		botlib_export->ea.EA_Say( args[1], VMAS(2) );
		return 0;
	case BOTLIB_EA_SAY_TEAM:
		botlib_export->ea.EA_SayTeam( args[1], VMAS(2) );
		return 0;
	case BOTLIB_EA_COMMAND:
		botlib_export->ea.EA_Command( args[1], VMAS(2) );
		return 0;

	case BOTLIB_EA_ACTION:
		botlib_export->ea.EA_Action( args[1], args[2] );
		break;
	case BOTLIB_EA_GESTURE:
		botlib_export->ea.EA_Gesture( args[1] );
		return 0;
	case BOTLIB_EA_TALK:
		botlib_export->ea.EA_Talk( args[1] );
		return 0;
	case BOTLIB_EA_ATTACK:
		botlib_export->ea.EA_Attack( args[1] );
		return 0;
	case BOTLIB_EA_ALT_ATTACK:
		botlib_export->ea.EA_Alt_Attack( args[1] );
		return 0;
	case BOTLIB_EA_FORCEPOWER:
		botlib_export->ea.EA_ForcePower( args[1] );
		return 0;
	case BOTLIB_EA_USE:
		botlib_export->ea.EA_Use( args[1] );
		return 0;
	case BOTLIB_EA_RESPAWN:
		botlib_export->ea.EA_Respawn( args[1] );
		return 0;
	case BOTLIB_EA_CROUCH:
		botlib_export->ea.EA_Crouch( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_UP:
		botlib_export->ea.EA_MoveUp( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_DOWN:
		botlib_export->ea.EA_MoveDown( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_FORWARD:
		botlib_export->ea.EA_MoveForward( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_BACK:
		botlib_export->ea.EA_MoveBack( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_LEFT:
		botlib_export->ea.EA_MoveLeft( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_RIGHT:
		botlib_export->ea.EA_MoveRight( args[1] );
		return 0;

	case BOTLIB_EA_SELECT_WEAPON:
		botlib_export->ea.EA_SelectWeapon( args[1], args[2] );
		return 0;
	case BOTLIB_EA_JUMP:
		botlib_export->ea.EA_Jump( args[1] );
		return 0;
	case BOTLIB_EA_DELAYED_JUMP:
		botlib_export->ea.EA_DelayedJump( args[1] );
		return 0;
	case BOTLIB_EA_MOVE:
		botlib_export->ea.EA_Move( args[1], VMAP(2, const vec_t, 3), VMF(3) );
		return 0;
	case BOTLIB_EA_VIEW:
		botlib_export->ea.EA_View( args[1], VMAP(2, const vec_t, 3) );
		return 0;

	case BOTLIB_EA_END_REGULAR:
		botlib_export->ea.EA_EndRegular( args[1], VMF(2) );
		return 0;
	case BOTLIB_EA_GET_INPUT:
		botlib_export->ea.EA_GetInput( args[1], VMF(2), VMAV(3, bot_input_t) );
		return 0;
	case BOTLIB_EA_RESET_INPUT:
		botlib_export->ea.EA_ResetInput( args[1] );
		return 0;

	case BOTLIB_AI_LOAD_CHARACTER:
		return botlib_export->ai.BotLoadCharacter( VMAS(1), VMF(2) );
	case BOTLIB_AI_FREE_CHARACTER:
		botlib_export->ai.BotFreeCharacter( args[1] );
		return 0;
	case BOTLIB_AI_CHARACTERISTIC_FLOAT:
		return FloatAsInt( botlib_export->ai.Characteristic_Float( args[1], args[2] ) );
	case BOTLIB_AI_CHARACTERISTIC_BFLOAT:
		return FloatAsInt( botlib_export->ai.Characteristic_BFloat( args[1], args[2], VMF(3), VMF(4) ) );
	case BOTLIB_AI_CHARACTERISTIC_INTEGER:
		return botlib_export->ai.Characteristic_Integer( args[1], args[2] );
	case BOTLIB_AI_CHARACTERISTIC_BINTEGER:
		return botlib_export->ai.Characteristic_BInteger( args[1], args[2], args[3], args[4] );
	case BOTLIB_AI_CHARACTERISTIC_STRING:
		botlib_export->ai.Characteristic_String( args[1], args[2], VMAP(3, char, args[4]), args[4] );
		return 0;

	case BOTLIB_AI_ALLOC_CHAT_STATE:
		return botlib_export->ai.BotAllocChatState();
	case BOTLIB_AI_FREE_CHAT_STATE:
		botlib_export->ai.BotFreeChatState( args[1] );
		return 0;
	case BOTLIB_AI_QUEUE_CONSOLE_MESSAGE:
		botlib_export->ai.BotQueueConsoleMessage( args[1], args[2], VMAS(3) );
		return 0;
	case BOTLIB_AI_REMOVE_CONSOLE_MESSAGE:
		botlib_export->ai.BotRemoveConsoleMessage( args[1], args[2] );
		return 0;
	case BOTLIB_AI_NEXT_CONSOLE_MESSAGE:
		return botlib_export->ai.BotNextConsoleMessage( args[1], VMAIV(2, struct bot_consolemessage_s, bot_consolemessage_size) );
	case BOTLIB_AI_NUM_CONSOLE_MESSAGE:
		return botlib_export->ai.BotNumConsoleMessages( args[1] );
	case BOTLIB_AI_INITIAL_CHAT:
		botlib_export->ai.BotInitialChat( args[1], VMAS(2), args[3], VMAS(4), VMAS(5), VMAS(6), VMAS(7), VMAS(8), VMAS(9), VMAS(10), VMAS(11) );
		return 0;
	case BOTLIB_AI_NUM_INITIAL_CHATS:
		return botlib_export->ai.BotNumInitialChats( args[1], VMAS(2) );
	case BOTLIB_AI_REPLY_CHAT:
		return botlib_export->ai.BotReplyChat( args[1], VMAS(2), args[3], args[4], VMAS(5), VMAS(6), VMAS(7), VMAS(8), VMAS(9), VMAS(10), VMAS(11), VMAS(12) );
	case BOTLIB_AI_CHAT_LENGTH:
		return botlib_export->ai.BotChatLength( args[1] );
	case BOTLIB_AI_ENTER_CHAT:
		botlib_export->ai.BotEnterChat( args[1], args[2], args[3] );
		return 0;
	case BOTLIB_AI_GET_CHAT_MESSAGE:
		botlib_export->ai.BotGetChatMessage( args[1], VMAP(2, char, args[3]), args[3] );
		return 0;
	case BOTLIB_AI_STRING_CONTAINS:
		return botlib_export->ai.StringContains( VMAS(1), VMAS(2), args[3] );
	case BOTLIB_AI_FIND_MATCH:
		return botlib_export->ai.BotFindMatch( VMAS(1), VMAIV(2, struct bot_match_s, bot_match_size), args[3] );
	case BOTLIB_AI_MATCH_VARIABLE:
		botlib_export->ai.BotMatchVariable( VMAIV(1, const struct bot_match_s, bot_match_size), args[2], VMAP(3, char, args[4]), args[4] );
		return 0;
	case BOTLIB_AI_UNIFY_WHITE_SPACES:
		botlib_export->ai.UnifyWhiteSpaces( VMAS(1) );
		return 0;
	case BOTLIB_AI_REPLACE_SYNONYMS:
		botlib_export->ai.BotReplaceSynonyms( VMAS(1), args[2] );
		return 0;
	case BOTLIB_AI_LOAD_CHAT_FILE:
		return botlib_export->ai.BotLoadChatFile( args[1], VMAS(2), VMAS(3) );
	case BOTLIB_AI_SET_CHAT_GENDER:
		botlib_export->ai.BotSetChatGender( args[1], args[2] );
		return 0;
	case BOTLIB_AI_SET_CHAT_NAME:
		botlib_export->ai.BotSetChatName( args[1], VMAS(2), args[3] );
		return 0;

	case BOTLIB_AI_RESET_GOAL_STATE:
		botlib_export->ai.BotResetGoalState( args[1] );
		return 0;
	case BOTLIB_AI_RESET_AVOID_GOALS:
		botlib_export->ai.BotResetAvoidGoals( args[1] );
		return 0;
	case BOTLIB_AI_REMOVE_FROM_AVOID_GOALS:
		botlib_export->ai.BotRemoveFromAvoidGoals( args[1], args[2] );
		return 0;
	case BOTLIB_AI_PUSH_GOAL:
		botlib_export->ai.BotPushGoal( args[1], VMAIV(2, struct bot_goal_s, bot_goal_size) );
		return 0;
	case BOTLIB_AI_POP_GOAL:
		botlib_export->ai.BotPopGoal( args[1] );
		return 0;
	case BOTLIB_AI_EMPTY_GOAL_STACK:
		botlib_export->ai.BotEmptyGoalStack( args[1] );
		return 0;
	case BOTLIB_AI_DUMP_AVOID_GOALS:
		botlib_export->ai.BotDumpAvoidGoals( args[1] );
		return 0;
	case BOTLIB_AI_DUMP_GOAL_STACK:
		botlib_export->ai.BotDumpGoalStack( args[1] );
		return 0;
	case BOTLIB_AI_GOAL_NAME:
		botlib_export->ai.BotGoalName( args[1], VMAP(2, char, args[3]), args[3] );
		return 0;
	case BOTLIB_AI_GET_TOP_GOAL:
		return botlib_export->ai.BotGetTopGoal( args[1], VMAIV(2, struct bot_goal_s, bot_goal_size) );
	case BOTLIB_AI_GET_SECOND_GOAL:
		return botlib_export->ai.BotGetSecondGoal( args[1], VMAIV(2, struct bot_goal_s, bot_goal_size) );
	case BOTLIB_AI_CHOOSE_LTG_ITEM:
		return botlib_export->ai.BotChooseLTGItem( args[1], VMAP(2, const vec_t, 3), VMAA(3, int, MAX_ITEMS), args[4] );
	case BOTLIB_AI_CHOOSE_NBG_ITEM:
		return botlib_export->ai.BotChooseNBGItem( args[1], VMAP(2, const vec_t, 3), VMAA(3, const int, MAX_ITEMS), args[4], VMAIV(5, const struct bot_goal_s, bot_goal_size), VMF(6) );
	case BOTLIB_AI_TOUCHING_GOAL:
		return botlib_export->ai.BotTouchingGoal( VMAP(1, const vec_t, 3), VMAIV(2, const struct bot_goal_s, bot_goal_size) );
	case BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE:
		return botlib_export->ai.BotItemGoalInVisButNotVisible( args[1], VMAP(2, const vec_t, 3), VMAP(3, const vec_t, 3), VMAIV(4, const struct bot_goal_s, bot_goal_size) );
	case BOTLIB_AI_GET_LEVEL_ITEM_GOAL:
		return botlib_export->ai.BotGetLevelItemGoal( args[1], VMAS(2), VMAIV(3, struct bot_goal_s, bot_goal_size) );
	case BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL:
		return botlib_export->ai.BotGetNextCampSpotGoal( args[1], VMAIV(2, struct bot_goal_s, bot_goal_size) );
	case BOTLIB_AI_GET_MAP_LOCATION_GOAL:
		return botlib_export->ai.BotGetMapLocationGoal( VMAS(1), VMAIV(2, struct bot_goal_s, bot_goal_size) );
	case BOTLIB_AI_AVOID_GOAL_TIME:
		return FloatAsInt( botlib_export->ai.BotAvoidGoalTime( args[1], args[2] ) );
	case BOTLIB_AI_SET_AVOID_GOAL_TIME:
		botlib_export->ai.BotSetAvoidGoalTime( args[1], args[2], VMF(3));
		return 0;
	case BOTLIB_AI_INIT_LEVEL_ITEMS:
		botlib_export->ai.BotInitLevelItems();
		return 0;
	case BOTLIB_AI_UPDATE_ENTITY_ITEMS:
		botlib_export->ai.BotUpdateEntityItems();
		return 0;
	case BOTLIB_AI_LOAD_ITEM_WEIGHTS:
		return botlib_export->ai.BotLoadItemWeights( args[1], VMAS(2) );
	case BOTLIB_AI_FREE_ITEM_WEIGHTS:
		botlib_export->ai.BotFreeItemWeights( args[1] );
		return 0;
	case BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC:
		botlib_export->ai.BotInterbreedGoalFuzzyLogic( args[1], args[2], args[3] );
		return 0;
	case BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC:
		botlib_export->ai.BotSaveGoalFuzzyLogic( args[1], VMAS(2) );
		return 0;
	case BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC:
		botlib_export->ai.BotMutateGoalFuzzyLogic( args[1], VMF(2) );
		return 0;
	case BOTLIB_AI_ALLOC_GOAL_STATE:
		return botlib_export->ai.BotAllocGoalState( args[1] );
	case BOTLIB_AI_FREE_GOAL_STATE:
		botlib_export->ai.BotFreeGoalState( args[1] );
		return 0;

	case BOTLIB_AI_RESET_MOVE_STATE:
		botlib_export->ai.BotResetMoveState( args[1] );
		return 0;
	case BOTLIB_AI_ADD_AVOID_SPOT:
		botlib_export->ai.BotAddAvoidSpot( args[1], VMAP(2, const vec_t, 3), VMF(3), args[4] );
		return 0;
	case BOTLIB_AI_MOVE_TO_GOAL:
		botlib_export->ai.BotMoveToGoal( VMAIV(1, struct bot_moveresult_s, bot_moveresult_size), args[2], VMAIV(3, const struct bot_goal_s, bot_goal_size), args[4] );
		return 0;
	case BOTLIB_AI_MOVE_IN_DIRECTION:
		return botlib_export->ai.BotMoveInDirection( args[1], VMAP(2, const vec_t, 3), VMF(3), args[4] );
	case BOTLIB_AI_RESET_AVOID_REACH:
		botlib_export->ai.BotResetAvoidReach( args[1] );
		return 0;
	case BOTLIB_AI_RESET_LAST_AVOID_REACH:
		botlib_export->ai.BotResetLastAvoidReach( args[1] );
		return 0;
	case BOTLIB_AI_REACHABILITY_AREA:
		return botlib_export->ai.BotReachabilityArea( VMAP(1, const vec_t, 3), args[2] );
	case BOTLIB_AI_MOVEMENT_VIEW_TARGET:
		return botlib_export->ai.BotMovementViewTarget( args[1], VMAIV(2, const struct bot_goal_s, bot_goal_size), args[3], VMF(4), VMAP(5, vec_t, 3) );
	case BOTLIB_AI_PREDICT_VISIBLE_POSITION:
		return botlib_export->ai.BotPredictVisiblePosition( VMAP(1, const vec_t, 3), args[2], VMAIV(3, const struct bot_goal_s, bot_goal_size), args[4], VMAP(5, vec_t, 3) );
	case BOTLIB_AI_ALLOC_MOVE_STATE:
		return botlib_export->ai.BotAllocMoveState();
	case BOTLIB_AI_FREE_MOVE_STATE:
		botlib_export->ai.BotFreeMoveState( args[1] );
		return 0;
	case BOTLIB_AI_INIT_MOVE_STATE:
		botlib_export->ai.BotInitMoveState( args[1], VMAIV(2, const struct bot_initmove_s, bot_initmove_size) );
		return 0;

	case BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON:
		return botlib_export->ai.BotChooseBestFightWeapon( args[1], VMAA(2, const int, MAX_ITEMS) );
	case BOTLIB_AI_GET_WEAPON_INFO:
		botlib_export->ai.BotGetWeaponInfo( args[1], args[2], VMAIV(3, struct weaponinfo_s, weaponinfo_size) );
		return 0;
	case BOTLIB_AI_LOAD_WEAPON_WEIGHTS:
		return botlib_export->ai.BotLoadWeaponWeights( args[1], VMAS(2) );
	case BOTLIB_AI_ALLOC_WEAPON_STATE:
		return botlib_export->ai.BotAllocWeaponState();
	case BOTLIB_AI_FREE_WEAPON_STATE:
		botlib_export->ai.BotFreeWeaponState( args[1] );
		return 0;
	case BOTLIB_AI_RESET_WEAPON_STATE:
		botlib_export->ai.BotResetWeaponState( args[1] );
		return 0;

	case BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION:
		return botlib_export->ai.GeneticParentsAndChildSelection(args[1], VMAA(2, const float, args[1]), VMAV(3, int), VMAV(4, int), VMAV(5, int));

	case TRAP_MEMSET:
		Com_Memset( VMAP(1, char, args[3]), args[2], args[3] );
		return 0;

	case TRAP_MEMCPY:
		Com_Memcpy( VMAP(1, char, args[3]), VMAP(2, char, args[3]), args[3] );
		return 0;

	case TRAP_STRNCPY:
		return VM_strncpy( args[1], args[2], args[3] );

	case TRAP_SIN:
		return FloatAsInt( sinf( VMF(1) ) );

	case TRAP_COS:
		return FloatAsInt( cosf( VMF(1) ) );

	case TRAP_ATAN2:
		return FloatAsInt( atan2f( VMF(1), VMF(2) ) );

	case TRAP_SQRT:
		return FloatAsInt( VMF(1) < 0 ? 0 : sqrtf( VMF(1) ) );

	case TRAP_MATRIXMULTIPLY:
		MatrixMultiply( VMAP(1, const vec3_t, 3), VMAP(2, const vec3_t, 3), VMAP(3, vec3_t, 3) );
		return 0;

	case TRAP_ANGLEVECTORS:
		AngleVectors( VMAP(1, const vec_t, 3), VMAP(2, vec_t, 3), VMAP(3, vec_t, 3), VMAP(4, vec_t, 3) );
		return 0;

	case TRAP_PERPENDICULARVECTOR:
		PerpendicularVector( VMAP(1, vec_t, 3), VMAP(2, const vec_t, 3) );
		return 0;

	case TRAP_FLOOR:
		return FloatAsInt( floorf( VMF(1) ) );

	case TRAP_CEIL:
		return FloatAsInt( ceilf( VMF(1) ) );

	case G_G2_LISTBONES:
		G2API_ListBones((g2handle_t)args[1], args[2], args[3]);
		return 0;

	case G_G2_LISTSURFACES:
		G2API_ListSurfaces((g2handle_t)args[1], args[2]);
		return 0;

	case G_G2_HAVEWEGHOULMODELS:
		return G2API_HaveWeGhoul2Models((g2handle_t)args[1]);

	case G_G2_SETMODELS:
		return 0;

	case G_G2_GETBOLT:
		return G2API_GetBoltMatrix((g2handle_t)args[1], args[2], args[3], VMAV(4, mdxaBone_t), VMAP(5, const vec_t, 3), VMAP(6, const vec_t, 3), args[7], VMAA(8, const qhandle_t, G2API_GetMaxModelIndex(true) + 1), VMAP(9, const vec_t, 3));

	case G_G2_GETBOLT_NOREC:
		gG2_GBMNoReconstruct = qtrue;
		return G2API_GetBoltMatrix((g2handle_t)args[1], args[2], args[3], VMAV(4, mdxaBone_t), VMAP(5, const vec_t, 3), VMAP(6, const vec_t, 3), args[7], VMAA(8, const qhandle_t, G2API_GetMaxModelIndex(true) + 1), VMAP(9, const vec_t, 3));

	case G_G2_GETBOLT_NOREC_NOROT:
		gG2_GBMNoReconstruct = qtrue;
		gG2_GBMUseSPMethod = qtrue;
		return G2API_GetBoltMatrix((g2handle_t)args[1], args[2], args[3], VMAV(4, mdxaBone_t), VMAP(5, const vec_t, 3), VMAP(6, const vec_t, 3), args[7], VMAA(8, const qhandle_t, G2API_GetMaxModelIndex(true) + 1), VMAP(9, const vec_t, 3));

	case G_G2_INITGHOUL2MODEL:
		return	G2API_InitGhoul2Model(VMAV(1, g2handle_t), VMAS(2), args[3], (qhandle_t) args[4],
			(qhandle_t) args[5], args[6], args[7]);

	case G_G2_ADDBOLT:
		return	G2API_AddBolt((g2handle_t)args[1], args[2], VMAS(3));

	case G_G2_SETBOLTINFO:
		G2API_SetBoltInfo((g2handle_t)args[1], args[2], args[3]);
		return 0;

	case G_G2_ANGLEOVERRIDE:
		return G2API_SetBoneAngles((g2handle_t)args[1], args[2], VMAS(3), VMAP(4, vec_t, 3), args[5],
			(const Eorientations)args[6], (const Eorientations)args[7], (const Eorientations)args[8],
			VMAA(9, qhandle_t, args[2] + 1), args[10], args[11]);

	case G_G2_PLAYANIM:
		return G2API_SetBoneAnim((g2handle_t)args[1], args[2], VMAS(3), args[4], args[5],
								args[6], VMF(7), args[8], VMF(9), args[10]);

	case G_G2_GETGLANAME:
		{
			char *local;
			local = G2API_GetGLAName((g2handle_t)args[1], args[2]);
			if (local)
			{
				char *point = VMAP(3, char, strlen(local) + 1);
				strcpy(point, local);
			}
		}

		return 0;

	case G_G2_COPYGHOUL2INSTANCE:
		return G2API_CopyGhoul2Instance((g2handle_t)args[1], (g2handle_t)args[2], args[3]);

	case G_G2_COPYSPECIFICGHOUL2MODEL:
		G2API_CopySpecificG2Model((g2handle_t)args[1], args[2], (g2handle_t)args[3], args[4]);
		return 0;

	case G_G2_DUPLICATEGHOUL2INSTANCE:
		G2API_DuplicateGhoul2Instance((g2handle_t)args[1], VMAV(2, g2handle_t));
		return 0;

	case G_G2_HASGHOUL2MODELONINDEX:
		return G2API_HasGhoul2ModelOnIndex(VMAV(1, const g2handle_t), args[2]);

	case G_G2_REMOVEGHOUL2MODEL:
		return G2API_RemoveGhoul2Model(VMAV(1, g2handle_t), args[2]);

	case G_G2_CLEANMODELS:
		G2API_CleanGhoul2Models(VMAV(1, g2handle_t));
		return 0;

	case G_G2_COLLISIONDETECT:
#ifdef G2_COLLISION_ENABLED
		G2API_CollisionDetect(VMAA(1, CollisionRecord_t, MAX_G2_COLLISIONS),
			(g2handle_t)args[2],
			VMAP(3, const vec_t, 3),
			VMAP(4, const vec_t, 3),
			args[5],
			args[6],
			VMAP(7, const vec_t, 3),
			VMAP(8, const vec_t, 3),
			VMAP(9, const vec_t, 3),
			G2VertSpaceServer,
			args[10],
			args[11],
			VMF(12));
#endif
		return 0;

	case MVAPI_GET_VERSION:
		return (int)VM_GetGameversion(gvm);

	}

	if (VM_MVAPILevel(gvm) >= 1) {
		switch (args[0]) {
		case G_MVAPI_LOCATE_GAME_DATA:
		{
			qboolean ret = MVAPI_LocateGameData((mvsharedEntity_t *)VM_ArgArray(args[0], args[1], args[3], args[2]), args[2], args[3]);
			VM_LocateGameDataCheck( sv.gentitiesMV, sv.gentitySizeMV, sv.num_entities );
			return ret;
		}
		case G_MVAPI_GET_CONNECTIONLESSPACKET:
			return (int)MVAPI_GetConnectionlessPacket(VMAV(1, mvaddr_t), VMAP(2, char, args[3]), args[3]);
		case G_MVAPI_SEND_CONNECTIONLESSPACKET:
			return (int)MVAPI_SendConnectionlessPacket(VMAV(1, const mvaddr_t), VMAS(2));
		case MVAPI_CONTROL_FIXES:
			return (int)SV_MVAPI_ControlFixes(args[1]);
		}
	}

	if (VM_MVAPILevel(gvm) >= 2) {
		switch (args[0]) {
		case G_MVAPI_DISABLE_STRUCT_CONVERSION:
			return (int)MVAPI_DisableStructConversion((qboolean)!!args[1]);
		}
	}

	if (VM_MVAPILevel(gvm) >= 3) {
		switch (args[0]) {
		case MVAPI_FS_FLOCK:
			return (int)FS_FLock(args[1], (flockCmd_t)args[2], (qboolean)!!args[3], MODULE_GAME);
		case MVAPI_SET_VERSION:
			VM_SetGameversion( gvm, (mvversion_t)args[1] );
			return 0;
		}
	}

	if (VM_MVAPILevel(gvm) >= 4) {
		switch(args[0]) {
		case G_MVAPI_RESET_SERVER_TIME:
			return (int)SV_MVAPI_ResetServerTime((qboolean)!!args[1]);
		case G_MVAPI_ENABLE_PLAYERSNAPSHOTS:
			return (int)SV_MVAPI_EnablePlayerSnapshots((qboolean)!!args[1]);
		case G_MVAPI_ENABLE_SUBMODELBYPASS:
			return SV_MVAPI_EnableSubmodelBypass( (qboolean)!!args[1] );
		case MVAPI_PRINT:
			Com_Printf_MV( args[1], "%s", VMAS(2) );
			return 0;
		}
	}

	Com_Error( ERR_DROP, "Bad game system trap: %lli", (long long int)args[0] );
	return -1;
}

#endif // SV_GAME_SYSCALLS_H
