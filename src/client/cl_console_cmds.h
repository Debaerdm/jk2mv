#pragma once

// Client-side console commands

void CL_ForwardToServer_f(void);
void CL_Setenv_f(void);
void CL_Disconnect_f(void);
void CL_Reconnect_f(void);
void CL_Connect_f(void);
void CL_Rcon_f(void);
void CL_Silent_f(void);
void CL_Vid_Restart_f(void);
void CL_Snd_Restart_f(void);
void CL_OpenedPK3List_f(void);
void CL_ReferencedPK3List_f(void);
void CL_Configstrings_f(void);
void CL_Clientinfo_f(void);

// Command completion helpers
void CL_CompleteRedirect(char *args, int argNum);
void CL_CompleteDemoName(char *args, int argNum);
void CL_CompleteModelName(char *args, int argNum);
