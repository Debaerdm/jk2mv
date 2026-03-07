#include "client.h"
#include "cl_video.h"
#include "cl_filename_utils.h"

/*
===============
CL_Video_f

video
video [filename]
===============
*/
void CL_Video_f(void) {
	char filename[MAX_OSPATH];
	int i;

	if (!clc.demoplaying) {
		Com_Printf("The video command can only be used when playing back demos\n");
		return;
	}

	if (Cmd_Argc() == 2) {
		// explicit filename
		Com_sprintf(filename, MAX_OSPATH, "videos/%s.avi", Cmd_Argv(1));
	} else {
		// scan for a free filename
		for (i = 0; i <= 9999; i++) {
			CL_GenerateVideoFilename(i, filename, sizeof(filename));

			if (!FS_FileExists(filename))
				break; // file doesn't exist
		}

		if (i > 9999) {
			Com_Printf(S_COLOR_RED "ERROR: no free file names to create video\n");
			return;
		}
	}

	CL_OpenAVIForWriting(filename);
}

/*
===============
CL_StopVideo_f
===============
*/
void CL_StopVideo_f(void) {
	CL_CloseAVI();
}
