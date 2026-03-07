#include "client.h"
#include "cl_reliable_cmd.h"

/*
======================
CL_AddReliableCommand

The given command will be transmitted to the server, and is guaranteed to
not have future usercmd_t executed before it is executed
======================
*/
void CL_AddReliableCommand(const char *cmd) {
	int index;

	// if we would be losing an old command that hasn't been acknowledged,
	// we must drop the connection
	if (clc.reliableSequence - clc.reliableAcknowledge > MAX_RELIABLE_COMMANDS) {
		Com_Error(ERR_DROP, "Client command overflow");
	}
	clc.reliableSequence++;
	index = clc.reliableSequence & (MAX_RELIABLE_COMMANDS - 1);
	Q_strncpyz(clc.reliableCommands[index], cmd, sizeof(clc.reliableCommands[index]));
}

/*
======================
CL_ChangeReliableCommand
======================
*/
void CL_ChangeReliableCommand(void) {
	int r, index, l;

	r = clc.reliableSequence - ((int)(qrandom()) * 5);
	index = clc.reliableSequence & (MAX_RELIABLE_COMMANDS - 1);
	l = (int)strlen(clc.reliableCommands[index]);
	if (l >= MAX_STRING_CHARS - 1) {
		l = MAX_STRING_CHARS - 2;
	}
	clc.reliableCommands[index][l] = '\n';
	clc.reliableCommands[index][l + 1] = '\0';
}
