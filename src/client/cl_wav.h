// cl_wav.h -- the WAV header of video_mp4's sound track
//
// No engine dependency, so it can be unit tested.

#ifndef CL_WAV_H
#define CL_WAV_H

#include <stdint.h>

#define WAV_HEADER_SIZE		44

static inline void CL_WAVPut16( unsigned char *p, unsigned int v ) {
	p[0] = (unsigned char)v;
	p[1] = (unsigned char)( v >> 8 );
}

static inline void CL_WAVPut32( unsigned char *p, uint32_t v ) {
	CL_WAVPut16( p, v & 0xFFFF );
	CL_WAVPut16( p + 2, v >> 16 );
}

/*
The header of a PCM WAV file whose data, dataBytes long, ends the file.
Past 4 GB the sizes don't fit: both are then 0xFFFFFFFF, which ffmpeg takes
as unknown and reads the data to the end of the file. It reads exactly the
size given otherwise, so a saturated size would cut the sound at 4 GB.
*/
static inline void CL_WAVHeader( unsigned char *out, int64_t dataBytes, int rate, int channels, int bits ) {
	const int		sampleSize = bits / 8 * channels;
	const uint32_t	data = dataBytes > 0xFFFFFFFFll - 36 ? 0xFFFFFFFFu : (uint32_t)dataBytes;

	out[0] = 'R'; out[1] = 'I'; out[2] = 'F'; out[3] = 'F';
	CL_WAVPut32( out + 4, data == 0xFFFFFFFFu ? data : 36 + data );
	out[8] = 'W'; out[9] = 'A'; out[10] = 'V'; out[11] = 'E';
	out[12] = 'f'; out[13] = 'm'; out[14] = 't'; out[15] = ' ';
	CL_WAVPut32( out + 16, 16 );
	CL_WAVPut16( out + 20, 1 );						// PCM
	CL_WAVPut16( out + 22, channels );
	CL_WAVPut32( out + 24, rate );
	CL_WAVPut32( out + 28, rate * sampleSize );		// bytes per second
	CL_WAVPut16( out + 32, sampleSize );				// block align
	CL_WAVPut16( out + 34, bits );
	out[36] = 'd'; out[37] = 'a'; out[38] = 't'; out[39] = 'a';
	CL_WAVPut32( out + 40, data );
}

#endif // CL_WAV_H
