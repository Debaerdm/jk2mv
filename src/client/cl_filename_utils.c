#include "client.h"
#include "cl_filename_utils.h"

/*
==================
CL_DemoFilename

Generates a sequential demo filename (demo0000 to demo9999)
==================
*/
void CL_DemoFilename(int number, char *fileName) {
	if (number < 0 || number > 9999) {
		Com_sprintf(fileName, MAX_OSPATH, "demo9999");
		return;
	}

	Com_sprintf(fileName, MAX_OSPATH, "demo%04d", number);
}

/*
==================
CL_GenerateVideoFilename

Generates a sequential video filename (video0000.avi to video9999.avi)
==================
*/
void CL_GenerateVideoFilename(int number, char *filename, size_t maxlen) {
	if (number < 0 || number > 9999) {
		Com_sprintf(filename, maxlen, "videos/video9999.avi");
		return;
	}

	Com_sprintf(filename, maxlen, "videos/video%04d.avi", number);
}
