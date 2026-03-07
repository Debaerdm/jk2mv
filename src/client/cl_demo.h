#pragma once

// Demo recording and playback subsystem

void CL_WriteDemoMessage(msg_t *msg, int headerBytes);
void CL_StopRecord_f(void);
void CL_Record_f(void);
void CL_DemoCompleted(void);
void CL_ReadDemoMessage(void);
void CL_PlayDemo_f(void);
void CL_StartDemoLoop(void);
void CL_NextDemo(void);

// Demo version detection helper
qboolean CL_ServerVersionIs103(const char *versionstr);

extern bool demoCheckFor103;
