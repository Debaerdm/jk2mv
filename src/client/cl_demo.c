#include "client.h"
#include "cl_demo.h"
#include "cl_filename_utils.h"

bool demoCheckFor103 = false;

/*
====================
CL_ServerVersionIs103
====================
*/
qboolean CL_ServerVersionIs103(const char *versionstr) {
	return strstr(versionstr, "v1.03") ? qtrue : qfalse;
}

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length
====================
*/
void CL_WriteDemoMessage(msg_t *msg, int headerBytes) {
	int len, swlen;

	// write the packet sequence
	len = clc.serverMessageSequence;
	swlen = LittleLong(len);
	FS_Write(&swlen, 4, clc.demofile);

	// skip the packet sequencing information
	len = msg->cursize - headerBytes;
	swlen = LittleLong(len);
	FS_Write(&swlen, 4, clc.demofile);
	FS_Write(msg->data + headerBytes, len, clc.demofile);
}

/*
====================
CL_StopRecording_f

stop recording a demo
====================
*/
void CL_StopRecord_f(void) {
	int len;

	if (!clc.demorecording) {
		Com_Printf("Not recording a demo.\n");
		return;
	}

	// finish up
	len = -1;
	FS_Write(&len, 4, clc.demofile);
	FS_Write(&len, 4, clc.demofile);
	FS_FCloseFile(clc.demofile);
	clc.demofile = 0;
	clc.demorecording = qfalse;
	clc.spDemoRecording = qfalse;
	Com_Printf("Stopped demo.\n");
}

/*
====================
CL_Record_f

record <demoname>

Begins recording a demo from the current position
====================
*/
void CL_Record_f(void) {
	char name[MAX_OSPATH];
	byte bufData[MAX_MSGLEN];
	msg_t buf;
	int i;
	int len;
	entityState_t *ent;
	entityState_t nullstate;
	char *s;
	char demoName[MAX_OSPATH];

	if (Cmd_Argc() > 2) {
		Com_Printf("record <demoname>\n");
		return;
	}

	if (clc.demorecording) {
		if (!clc.spDemoRecording) {
			Com_Printf("Already recording.\n");
		}
		return;
	}

	if (cls.state != CA_ACTIVE) {
		Com_Printf("You must be in a level to record.\n");
		return;
	}

	if (Cmd_Argc() == 2) {
		s = Cmd_Argv(1);
		Q_strncpyz(demoName, s, sizeof(demoName));
		Com_sprintf(name, sizeof(name), "demos/%s.dm_%d", demoName,
			MV_GetCurrentProtocol());
	} else {
		int number;

		// scan for a free demo name
		for (number = 0; number <= 9999; number++) {
			CL_DemoFilename(number, demoName);
			Com_sprintf(name, sizeof(name), "demos/%s.dm_%d", demoName,
				MV_GetCurrentProtocol());

			len = FS_ReadFile(name, NULL);
			if (len <= 0) {
				break; // file doesn't exist
			}
		}
	}

	// open the demo file

	Com_Printf("recording to %s.\n", name);
	clc.demofile = FS_FOpenFileWrite(name);
	if (!clc.demofile) {
		Com_Printf("ERROR: couldn't open.\n");
		return;
	}
	clc.demorecording = qtrue;
	if (Cvar_VariableValue("ui_recordSPDemo")) {
		clc.spDemoRecording = qtrue;
	} else {
		clc.spDemoRecording = qfalse;
	}

	Q_strncpyz(clc.demoName, demoName, sizeof(clc.demoName));

	// don't start saving messages until a non-delta compressed message is received
	clc.demowaiting = qtrue;

	// write out the gamestate message
	MSG_Init(&buf, bufData, sizeof(bufData));
	MSG_Bitstream(&buf);

	// NOTE, MRE: all server->client messages now acknowledge
	MSG_WriteLong(&buf, clc.reliableSequence);

	MSG_WriteByte(&buf, svc_gamestate);
	MSG_WriteLong(&buf, clc.serverCommandSequence);

	// configstrings
	for (i = 0; i < MAX_CONFIGSTRINGS; i++) {
		if (!cl.gameState.stringOffsets[i]) {
			continue;
		}
		s = cl.gameState.stringData + cl.gameState.stringOffsets[i];
		MSG_WriteByte(&buf, svc_configstring);
		MSG_WriteShort(&buf, i);
		MSG_WriteBigString(&buf, s);
	}

	// baselines
	Com_Memset(&nullstate, 0, sizeof(nullstate));
	for (i = 0; i < MAX_GENTITIES; i++) {
		ent = &cl.entityBaselines[i];
		if (!ent->number) {
			continue;
		}
		MSG_WriteByte(&buf, svc_baseline);
		MSG_WriteDeltaEntity(&buf, &nullstate, ent, qtrue);
	}

	MSG_WriteByte(&buf, svc_EOF);

	// finished writing the gamestate stuff

	// write the client num
	MSG_WriteLong(&buf, clc.clientNum);
	// write the checksum feed
	MSG_WriteLong(&buf, clc.checksumFeed);

	// finished writing the client packet
	MSG_WriteByte(&buf, svc_EOF);

	// write it to the demo file
	len = LittleLong(clc.serverMessageSequence - 1);
	FS_Write(&len, 4, clc.demofile);

	len = LittleLong(buf.cursize);
	FS_Write(&len, 4, clc.demofile);
	FS_Write(buf.data, buf.cursize, clc.demofile);

	// the rest of the demo file will be copied from net messages
}

/*
=================
CL_DemoCompleted
=================
*/
void CL_DemoCompleted(void) {
	if (cl_timedemo && cl_timedemo->integer) {
		int time;

		time = Sys_Milliseconds() - clc.timeDemoStart;
		if (time > 0) {
			Com_Printf("%i frames, %3.1f seconds: %3.1f fps\n", clc.timeDemoFrames,
				time / 1000.0, clc.timeDemoFrames * 1000.0 / time);
		}
	}

	CL_NextDemo();
	CL_Disconnect_f();
	// disconnect here does long jump, don't put anything after
}

/*
=================
CL_ReadDemoMessage
=================
*/
void CL_ReadDemoMessage(void) {
	int r;
	msg_t buf;
	byte bufData[MAX_MSGLEN];
	int s;

	if (!clc.demofile) {
		CL_DemoCompleted();
		return;
	}

	// get the sequence number
	r = FS_Read(&s, 4, clc.demofile);
	if (r != 4) {
		CL_DemoCompleted();
		return;
	}
	clc.serverMessageSequence = LittleLong(s);

	// init the message
	MSG_Init(&buf, bufData, sizeof(bufData));

	// get the length
	r = FS_Read(&buf.cursize, 4, clc.demofile);
	if (r != 4) {
		CL_DemoCompleted();
		return;
	}
	buf.cursize = LittleLong(buf.cursize);
	if (buf.cursize == -1) {
		CL_DemoCompleted();
		return;
	}
	if (buf.cursize > buf.maxsize) {
		Com_Error(ERR_DROP, "CL_ReadDemoMessage: demoMsglen > MAX_MSGLEN");
	}
	r = FS_Read(buf.data, buf.cursize, clc.demofile);
	if (r != buf.cursize) {
		Com_Printf("Demo file was truncated.\n");
		CL_DemoCompleted();
		return;
	}

	clc.lastPacketTime = cls.realtime;
	buf.readcount = 0;
	CL_ParseServerMessage(&buf);
}

/*
====================
CL_PlayDemo_f

demo <demoname>
====================
*/
void CL_PlayDemo_f(void) {
	char name[MAX_OSPATH];
	char arg[MAX_OSPATH];

	if (Cmd_Argc() != 2) {
		Com_Printf("demo <demoname>\n");
		return;
	}

	Q_strncpyz(arg, Cmd_Argv(1), sizeof(arg));

	// make sure a local server is killed
	Cvar_Set("sv_killserver", "1");
	SV_Frame(0);

	CL_Disconnect(qtrue);

	// open the demo file
	if (!Q_stricmp(arg + strlen(arg) - strlen(".dm_15"), ".dm_15") ||
		!Q_stricmp(arg + strlen(arg) - strlen(".dm_16"), ".dm_16")) {
		// Load "dm_15" and "dm_16" demos.
		Com_sprintf(name, sizeof(name), "demos/%s", arg);

		FS_FOpenFileRead(name, &clc.demofile, qtrue);
		if (!clc.demofile) {
			if (!Q_stricmp(arg, "(null)")) {
				Com_Error(ERR_DROP, "%s",
					SP_GetStringTextString("CON_TEXT_NO_DEMO_SELECTED"));
			} else {
				Com_Error(ERR_DROP, "couldn't open %s", name);
			}
			return;
		}
	} else {
		// Check for both, "dm_15" and "dm_16".
		Com_sprintf(name, sizeof(name), "demos/%s.dm_15", arg);
		FS_FOpenFileRead(name, &clc.demofile, qtrue);
		if (!clc.demofile) {
			Com_sprintf(name, sizeof(name), "demos/%s.dm_16", arg);
			FS_FOpenFileRead(name, &clc.demofile, qtrue);
			if (!clc.demofile) {
				if (!Q_stricmp(arg, "(null)")) {
					Com_Error(ERR_DROP, "%s",
						SP_GetStringTextString("CON_TEXT_NO_DEMO_SELECTED"));
				} else {
					Com_Error(ERR_DROP,
						"couldn't open demos/%s.dm_15 or demos/%s.dm_16", arg, arg);
				}
				return;
			}
		}
	}
	Q_strncpyz(clc.demoName, arg, sizeof(clc.demoName));

	Con_Close();

	cls.state = CA_CONNECTED;
	clc.demoplaying = qtrue;
	com_demoplaying = qtrue;

	Q_strncpyz(cls.servername, arg, sizeof(cls.servername));

	// Set the protocol according to the the demo-file.
	if (!Q_stricmp(name + strlen(name) - strlen(".dm_15"), ".dm_15")) {
		MV_SetCurrentGameversion(VERSION_1_02);
		demoCheckFor103 = true; // if this demo happens to be a 1.03 demo, check for
								// that in CL_ParseGamestate
	} else if (!Q_stricmp(name + strlen(name) - strlen(".dm_16"), ".dm_16")) {
		MV_SetCurrentGameversion(VERSION_1_04);
	}

	// read demo messages until connected
	while (cls.state >= CA_CONNECTED && cls.state < CA_PRIMED) {
		CL_ReadDemoMessage();
	}
	// don't get the first snapshot this frame, to prevent the long
	// time from the gamestate load from messing causing a time skip
	clc.firstDemoFrameSkipped = qfalse;
}

/*
====================
CL_StartDemoLoop

Closing the main menu will restart the demo loop
====================
*/
void CL_StartDemoLoop(void) {
	// start the demo loop again
	Cbuf_AddText("d1\n");
	cls.keyCatchers = 0;
}

/*
==================
CL_NextDemo

Called when a demo or cinematic finishes
If the "nextdemo" cvar is set, that command will be issued
==================
*/
void CL_NextDemo(void) {
	char v[MAX_STRING_CHARS];

	Q_strncpyz(v, Cvar_VariableString("nextdemo"), sizeof(v));
	v[MAX_STRING_CHARS - 1] = 0;
	Com_DPrintf("CL_NextDemo: %s\n", v);
	if (!v[0]) {
		return;
	}

	Cvar_Set("nextdemo", "");
	Cbuf_AddText(v);
	Cbuf_AddText("\n");
	Cbuf_Execute();
}
