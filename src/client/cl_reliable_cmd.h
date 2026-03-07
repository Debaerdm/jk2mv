#pragma once

// Reliable command communication (client → server)
// Commands transmitted via this system are guaranteed to reach the server
// and execute before any future usercmd_t

void CL_AddReliableCommand(const char *cmd);
void CL_ChangeReliableCommand(void);
