#include "client.h"
#include "cl_console_cmds.h"
#include "cl_reliable_cmd.h"

/*
==================
CL_ForwardToServer_f
==================
*/
void CL_ForwardToServer_f(void) {
	if (cls.state != CA_ACTIVE || clc.demoplaying) {
		Com_Printf("Not connected to a server.\n");
		return;
	}

	// don't forward the first argument
	if (Cmd_Argc() > 1) {
		CL_AddReliableCommand(Cmd_Args());
	}
}

/*
==================
CL_Setenv_f

Mostly for controlling voodoo environment variables
==================
*/
void CL_Setenv_f(void) {
	int argc = Cmd_Argc();

	if (argc > 2) {
		char buffer[1024];

		Q_strncpyz(buffer, Cmd_Argv(1), sizeof(buffer));
		Q_strcat(buffer, sizeof(buffer), "=");
		Q_strcat(buffer, sizeof(buffer), Cmd_ArgsFrom(2));

		if (putenv(buffer) != 0)
			Com_Printf("Unable to set environment variable\n");
	} else if (argc == 2) {
		char *env = getenv(Cmd_Argv(1));

		if (env) {
			Com_Printf("%s=%s\n", Cmd_Argv(1), env);
		} else {
			Com_Printf("%s undefined\n", Cmd_Argv(1));
		}
	}
}

/*
==================
CL_Disconnect_f
==================
*/
void CL_Disconnect_f(void) {
	SCR_StopCinematic();
	Cvar_Set("ui_singlePlayerActive", "0");
	if (cls.state != CA_DISCONNECTED && cls.state != CA_CINEMATIC) {
		Com_Error(ERR_DISCONNECT, "Disconnected from server");
	}
}

/*
================
CL_Reconnect_f
================
*/
void CL_Reconnect_f(void) {
	if (!strlen(cls.servername) || !strcmp(cls.servername, "localhost")) {
		Com_Printf("Can't reconnect to localhost.\n");
		return;
	}
	Cvar_Set("ui_singlePlayerActive", "0");
	Cbuf_AddText(va("connect %s\n", cls.servername));
}

/*
================
CL_Connect_f
================
*/
void CL_Connect_f(void) {
	char server[MAX_OSPATH];

	if (Cmd_Argc() != 2) {
		Com_Printf("usage: connect [server]\n");
		return;
	}

	Cvar_Set("ui_singlePlayerActive", "0");

	com_demoplaying = qfalse;

	// fire a message off to the motd server
	CL_RequestMotd();

	// clear any previous "server full" type messages
	clc.serverMessage[0] = 0;

	Q_strncpyz(server, Cmd_Argv(1), sizeof(server));

	if (com_sv_running->integer && !strcmp(server, "localhost")) {
		// if running a local server, kill it
		SV_Shutdown("Server quit");
	}

	// make sure a local server is killed
	Cvar_Set("sv_killserver", "1");
	SV_Frame(0);

	CL_Disconnect(qtrue);
	Con_Close();

	Q_strncpyz(cls.servername, server, sizeof(cls.servername));

	if (!NET_StringToAdr(cls.servername, &clc.serverAddress)) {
		Com_Printf("Bad server address\n");
		cls.state = CA_DISCONNECTED;
		return;
	}
	if (clc.serverAddress.port == 0) {
		clc.serverAddress.port = BigShort(PORT_SERVER);
	}
	Com_Printf("%s resolved to %i.%i.%i.%i:%i\n", cls.servername,
		clc.serverAddress.ip[0], clc.serverAddress.ip[1], clc.serverAddress.ip[2],
		clc.serverAddress.ip[3], BigShort(clc.serverAddress.port));

	// if we aren't playing on a lan, we need to authenticate
	// with the cd key
	if (NET_IsLocalAddress(clc.serverAddress)) {
		cls.state = CA_CHALLENGING;
	} else {
		cls.state = CA_CONNECTING;
	}
	clc.gotInfo = qfalse;
	clc.gotStatus = qfalse;
	clc.udpdl = 0;
	clc.httpdl[0] = 0;

	cls.keyCatchers = 0;
	clc.connectTime = -99999; // CL_CheckForResend() will fire immediately
	clc.connectPacketCount = 0;

	// server connection string
	Cvar_Set("cl_currentServerAddress", server);
}

#define MAX_RCON_MESSAGE 1024

/*
=====================
CL_Rcon_f

  Send the rest of the command line over as
  an unconnected command.
=====================
*/
void CL_Rcon_f(void) {
	char message[MAX_RCON_MESSAGE];

	if (!strlen(rcon_client_password->string)) {
		Com_Printf("You must set 'rconpassword' before\n"
				   "issuing an rcon command.\n");
		return;
	}

	message[0] = -1;
	message[1] = -1;
	message[2] = -1;
	message[3] = -1;
	message[4] = 0;

	Q_strcat(message, MAX_RCON_MESSAGE, "rcon ");

	Q_strcat(message, MAX_RCON_MESSAGE, rcon_client_password->string);
	Q_strcat(message, MAX_RCON_MESSAGE, " ");

	Q_strcat(message, MAX_RCON_MESSAGE, Cmd_Cmd() + 5);

	if (cls.state >= CA_CONNECTED) {
		rcon_address = clc.netchan.remoteAddress;
	} else {
		if (!strlen(rconAddress->string)) {
			Com_Printf("You must either be connected,\n"
					   "or set the 'rconAddress' cvar\n"
					   "to issue rcon commands\n");

			return;
		}
		NET_StringToAdr(rconAddress->string, &rcon_address);
		if (rcon_address.port == 0) {
			rcon_address.port = BigShort(PORT_SERVER);
		}
	}

	NET_SendPacket(NS_CLIENT, (int)strlen(message) + 1, message, rcon_address);
}

/*
============
CL_Silent_f
============
*/
void CL_Silent_f(void) {
	Com_BeginRedirect(NULL, 0, NULL, qtrue);

	Cmd_DropArg(0);
	Cmd_Execute();

	Com_EndRedirect();
}

/*
==================
CL_CompleteRedirect
==================
*/
void CL_CompleteRedirect(char *args, int argNum) {
	// skip first command
	char *p = Com_SkipTokens(args, 1, " ");

	if (p > args)
		Field_CompleteCommand(p, qtrue, qtrue, qtrue);
}

/*
==================
CL_CompleteDemoName
==================
*/
void CL_CompleteDemoName(char *args, int argNum) {
	if (argNum == 2)
		Field_CompleteFilename("demos", ".dm_15|.dm_16", qfalse);
}

/*
==================
CL_CompleteModelName
==================
*/
void CL_CompleteModelName(char *args, int argNum) {
	if (argNum == 2)
		Field_CompleteModelname();
}

/*
=================
CL_Vid_Restart_f

Restart the video subsystem

we also have to reload the UI and CGame because the renderer
doesn't know what graphics to reload
=================
*/
void CL_Vid_Restart_f(void) {

	// Settings may have changed so stop recording now
	CL_CloseAVI();
	// don't let them loop during the restart
	S_StopAllSounds();
	// shutdown the UI
	CL_ShutdownUI();
	// shutdown the CGame
	CL_ShutdownCGame();
	// shutdown the renderer and clear the renderer interface
	CL_ShutdownRef();
	// client is no longer pure untill new checksums are sent
	CL_ResetPureClientAtServer();
	// clear pak references
	FS_ClearPakReferences(FS_UI_REF | FS_CGAME_REF);
	// reinitialize the filesystem if the game directory or checksum has changed
	FS_ConditionalRestart(clc.checksumFeed);

	cls.rendererStarted = qfalse;
	cls.uiStarted = qfalse;
	cls.cgameStarted = qfalse;
	cls.soundRegistered = qfalse;

	// unpause so the cgame definately gets a snapshot and renders a frame
	Cvar_Set("cl_paused", "0");

	// if not running a server clear the whole hunk
	if (!com_sv_running->integer) {
		CM_ClearMap();
		// clear the whole hunk
		Hunk_Clear();
	} else {
		// clear all the client data on the hunk
		Hunk_ClearToMark();
	}

	// initialize the renderer interface
	CL_InitRef();

	// startup all the client stuff
	CL_StartHunkUsers();

	// start the cgame if connected
	if (cls.state > CA_CONNECTED && cls.state != CA_CINEMATIC) {
		cls.cgameStarted = qtrue;
		CL_InitCGame();
		// send pure checksums
		CL_SendPureChecksums();
	}

	// Reapply mvremaps to override classic ones
	CL_ShaderStateChanged();
}

/*
=================
CL_Snd_Restart_f

Restart the sound subsystem
The cgame and game must also be forced to restart because
handles will be invalid
=================
*/
extern void S_UnCacheDynamicMusic(void);
void CL_Snd_Restart_f(void) {
	S_Shutdown();
	S_Init();

	S_FreeAllSFXMem();
	S_UnCacheDynamicMusic();

	extern qboolean s_soundMuted;
	s_soundMuted = qfalse; // we can play again

	extern void S_RestartMusic(void);
	S_RestartMusic();
}

/*
==================
CL_OpenedPK3List_f
==================
*/
void CL_OpenedPK3List_f(void) {
	Com_Printf("Opened PK3 Names: %s\n", FS_LoadedPakNames());
}

/*
==================
CL_ReferencedPK3List_f
==================
*/
void CL_ReferencedPK3List_f(void) {
	Com_Printf("Referenced PK3 Names: %s\n", FS_ReferencedPakNames());
}

/*
==================
CL_Configstrings_f
==================
*/
void CL_Configstrings_f(void) {
	int i;
	int ofs;

	if (cls.state != CA_ACTIVE) {
		Com_Printf("Not connected to a server.\n");
		return;
	}

	for (i = 0; i < MAX_CONFIGSTRINGS; i++) {
		ofs = cl.gameState.stringOffsets[i];
		if (!ofs) {
			continue;
		}
		Com_Printf("%4i: %s\n", i, cl.gameState.stringData + ofs);
	}
}

/*
==============
CL_Clientinfo_f
==============
*/
void CL_Clientinfo_f(void) {
	Com_Printf("--------- Client Information ---------\n");
	Com_Printf("state: %i\n", cls.state);
	Com_Printf("Server: %s\n", cls.servername);
	Com_Printf("User info settings:\n");
	Info_Print(Cvar_InfoString(CVAR_USERINFO));
	Com_Printf("--------------------------------------\n");
}
