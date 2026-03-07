#include "client.h"
#include "cl_filename_utils.h"

/*
==================
CL_DemoFilename

Generates a sequential demo filename (demo0000 to demo9999)
==================
*/
void CL_DemoFilename(int number, char *fileName) {
	int a, b, c, d;

	if (number < 0 || number > 9999) {
		Com_sprintf(fileName, MAX_OSPATH, "demo9999.tga");
		return;
	}

	a = number / 1000;
	number -= a * 1000;
	b = number / 100;
	number -= b * 100;
	c = number / 10;
	number -= c * 10;
	d = number;

	Com_sprintf(fileName, MAX_OSPATH, "demo%i%i%i%i", a, b, c, d);
}

/*
==================
CL_GenerateVideoFilename

Generates a sequential video filename (video0000.avi to video9999.avi)
==================
*/
void CL_GenerateVideoFilename(int number, char *filename, size_t maxlen) {
	int a, b, c, d;
	int last = number;

	if (number < 0 || number > 9999) {
		Com_sprintf(filename, maxlen, "videos/video9999.avi");
		return;
	}

	a = last / 1000;
	last -= a * 1000;
	b = last / 100;
	last -= b * 100;
	c = last / 10;
	last -= c * 10;
	d = last;

	Com_sprintf(filename, maxlen, "videos/video%d%d%d%d.avi", a, b, c, d);
}
