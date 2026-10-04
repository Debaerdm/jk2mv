// cl_scrn.c -- master for refresh, status bar, console, chat, notify, etc

#include "client.h"
#include "snd_public.h"
#include <mv_setup.h>
#include <algorithm>

extern console_t con;
qboolean	scr_initialized;		// ready to draw

cvar_t		*cl_timegraph;
cvar_t		*cl_debuggraph;
cvar_t		*cl_graphheight;
cvar_t		*cl_graphscale;
cvar_t		*cl_graphshift;
cvar_t		*cl_perfOverlay;

static qboolean SCR_IsColorCode(const char *s)
{
	return (qboolean)(Q_IsColorString(s) || (MV_USE102COLOR && Q_IsColorString_1_02(s)));
}

static qboolean SCR_ShouldSkipCharDraw(int ch, float y, float minY)
{
	return (qboolean)(ch == ' ' || y < minY);
}

static void SCR_SetStringColorFromCode(vec4_t color, const float *setColor, const char *s)
{
	Com_Memcpy(color, g_color_table[ColorIndex(*(s + 1))], sizeof(vec4_t));
	color[3] = setColor[3];
	re.SetColor(color);
}

static void SCR_DrawDefaultConnectBackground(void)
{
	qhandle_t hShader = re.RegisterShader("menu/art/unknownmap");
	re.DrawStretchPic(0, 0, 640, 480, 0, 0, 1, 1, hShader, 1, 1);
}

static void SCR_RefreshUI(void)
{
	VM_Call(uivm, UI_REFRESH, cls.realtime);
}

static void SCR_DrawConnectScreen(qboolean overlay)
{
	SCR_RefreshUI();
	VM_Call(uivm, UI_DRAW_CONNECT_SCREEN, overlay);
}

/*
================
SCR_DrawNamedPic

Coordinates are 640*480 virtual values
=================
*/
void SCR_DrawNamedPic( float x, float y, float width, float height, const char *picname ) {
	qhandle_t	hShader;

	assert( width != 0 );

	hShader = re.RegisterShader( picname );
	re.DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader, 1, 1 );
}


/*
================
SCR_FillRect

Coordinates are 640*480 virtual values
=================
*/
void SCR_FillRect( float x, float y, float width, float height, const float *color ) {
	re.SetColor( color );

	re.DrawStretchPic( x, y, width, height, 0, 0, 0, 0, cls.whiteShader, 1, 1 );

	re.SetColor( nullptr );
}


/*
================
SCR_DrawPic

Coordinates are 640*480 virtual values
=================
*/
void SCR_DrawPic( float x, float y, float width, float height, qhandle_t hShader ) {
	re.DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader, 1, 1 );
}



/*
** SCR_DrawChar
** chars are drawn at 640*480 virtual screen size
*/
static void SCR_DrawChar( int x, int y, float size, int ch ) {
	int row, col;
	float frow, fcol;
	float	ax, ay, aw, ah;

	ch &= 255;

	if ( SCR_ShouldSkipCharDraw( ch, y, -size ) ) {
		return;
	}

	ax = x;
	ay = y;
	aw = size;
	ah = size;

	row = ch>>4;
	col = ch&15;

	float size2;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.03125;
	size2 = 0.0625;

	re.DrawStretchPic( ax, ay, aw, ah,
					   fcol, frow,
					   fcol + size, frow + size2,
					   cls.charSetShader, 1, 1 );
}

/*
** SCR_DrawSmallChar
** small chars are drawn at native screen resolution
*/
void SCR_DrawSmallChar( int x, int y, int ch ) {
	int row, col;
	float frow, fcol;
	float size;

	ch &= 255;

	if ( SCR_ShouldSkipCharDraw( ch, y, -con.charHeight ) ) {
		return;
	}

	row = ch>>4;
	col = ch&15;

	float size2;

	frow = row*0.0625;
	fcol = col*0.0625;

#ifdef _JK2
	size = 0.03125;
#else
	size = 0.0625;
#endif
	size2 = 0.0625;

	re.DrawStretchPic( x, y, con.charWidth, con.charHeight,
					   fcol, frow,
					   fcol + size, frow + size2,
					   cls.charSetShader,
					   cls.xadjust, cls.yadjust );
}


/*
==================
SCR_DrawBigString[Color]

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
static void SCR_DrawStringExt( int x, int y, float size, const char *string, const float *setColor, qboolean forceColor ) {
	vec4_t		color;
	const char	*s;
	int			xx;

	// draw the drop shadow
	color[0] = color[1] = color[2] = 0;
	color[3] = setColor[3];
	re.SetColor( color );
	s = string;
	xx = x;
	while ( *s ) {
		if ( SCR_IsColorCode( s ) ) {
			s += 2;
			continue;
		}
		SCR_DrawChar( xx+2, y+2, size, *s );
		xx += size;
		s++;
	}


	// draw the colored text
	s = string;
	xx = x;
	re.SetColor( setColor );
	while ( *s ) {
		if ( SCR_IsColorCode( s ) ) {
			if ( !forceColor ) {
				SCR_SetStringColorFromCode( color, setColor, s );
			}
			s += 2;
			continue;
		}
		SCR_DrawChar( xx, y, size, *s );
		xx += size;
		s++;
	}
	re.SetColor( nullptr );
}


void SCR_DrawBigString( int x, int y, const char *s, float alpha ) {
	float	color[4];

	color[0] = color[1] = color[2] = 1.0;
	color[3] = alpha;
	SCR_DrawStringExt( x, y, BIGCHAR_WIDTH, s, color, qfalse );
}

void SCR_DrawBigStringColor( int x, int y, const char *s, const vec4_t color ) {
	SCR_DrawStringExt( x, y, BIGCHAR_WIDTH, s, color, qtrue );
}


/*
==================
SCR_DrawSmallString[Color]

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
void SCR_DrawSmallStringExt( int x, int y, const char *string, const vec4_t setColor, qboolean forceColor ) {
	vec4_t		color;
	const char	*s;
	int			xx;

	// draw the colored text
	s = string;
	xx = x;
	re.SetColor( setColor );
	while ( *s ) {
		if ( SCR_IsColorCode( s ) ) {
			if ( !forceColor ) {
				SCR_SetStringColorFromCode( color, setColor, s );
			}
			s += 2;
			continue;
		}
		SCR_DrawSmallChar( xx, y, *s );
		xx += con.charWidth;
		s++;
	}
	re.SetColor( nullptr );
}



/*
** SCR_Strlen -- skips color escape codes
*/
static int SCR_Strlen( const char *str ) {
	const char *s = str;
	int count = 0;

	while ( *s ) {
		if ( SCR_IsColorCode( s ) ) {
			s += 2;
		} else {
			count++;
			s++;
		}
	}

	return count;
}

/*
** SCR_GetBigStringWidth
*/
int	SCR_GetBigStringWidth( const char *str ) {
	return SCR_Strlen( str ) * 16;
}


//===============================================================================

/*
=================
SCR_DrawDemoRecording
=================
*/
void SCR_DrawDemoRecording( void ) {
	char	string[1024];
	int		pos;

	if ( !clc.demorecording ) {
		return;
	}
	if ( clc.spDemoRecording ) {
		return;
	}

	if (cl_drawRecording->integer >= 2 && cls.recordingShader) {
		static const float width = 60.0f, height = 15.0f;
		re.SetColor(nullptr);
		re.DrawStretchPic(0, cls.glconfig.vidHeight - height, width, height,
			0, 0, 1, 1, cls.recordingShader, cls.xadjust, cls.yadjust);
	} else if (cl_drawRecording->integer) {
		pos = FS_FTell( clc.demofile );
		Com_sprintf( string, sizeof( string ), "RECORDING %s: %ik", clc.demoName, pos / 1024 );
		SCR_DrawStringExt( 320 - (int)strlen( string ) * 4, 20, 8, string, g_color_table[7], qtrue );
	}
}


/*
===============================================================================

DEBUG GRAPH

===============================================================================
*/

typedef struct
{
	float	value;
	int		color;
} graphsamp_t;

static	int			current;
static	graphsamp_t	values[1024];

/*
==============
SCR_DebugGraph
==============
*/
void SCR_DebugGraph (float value, int color)
{
	values[current&1023].value = value;
	values[current&1023].color = color;
	current++;
}

/*
==============
SCR_DrawDebugGraph
==============
*/
void SCR_DrawDebugGraph (void)
{
	int		a, x, y, w, i, h;
	float	v;
	int		color;

	//
	// draw the graph
	//
	w = cls.glconfig.vidWidth;
	x = 0;
	y = cls.glconfig.vidHeight;
	re.SetColor( g_color_table[0] );
	re.DrawStretchPic(x, y - cl_graphheight->integer, w, cl_graphheight->integer,
		0, 0, 0, 0, cls.whiteShader, cls.xadjust, cls.yadjust );
	re.SetColor( nullptr );

	for (a=0 ; a<w ; a++)
	{
		i = (current-1-a+1024) & 1023;
		v = values[i].value;
		color = values[i].color;
		v = v * cl_graphscale->integer + cl_graphshift->integer;

		if (v < 0)
			v += cl_graphheight->integer * (1+(int)(-v / cl_graphheight->integer));
		h = (int)v % cl_graphheight->integer;
		re.DrawStretchPic( x+w-1-a, y - h, 1, h,
			0, 0, 0, 0, cls.whiteShader, cls.xadjust, cls.yadjust );
	}
}

/*
===============================================================================

PERFORMANCE OVERLAY

cl_perfOverlay 1 shows the frame rate, the 1% low and the frame time,
2 adds a frame time graph. Not a cheat: it only shows timings.

===============================================================================
*/

#define PERF_SAMPLES	512		// power of two

static int		perfFrameUsec[PERF_SAMPLES];
static unsigned	perfFrameCount;

static void SCR_RecordFrameTime( void ) {
	static int64_t	lastFrame;
	const int64_t	now = Sys_Microseconds();

	if ( lastFrame ) {
		// clamped at 10 s, so a long stall (debugger, suspend) can't overflow
		perfFrameUsec[perfFrameCount & ( PERF_SAMPLES - 1 )] = (int)MIN( now - lastFrame, (int64_t)10000000 );
		perfFrameCount++;
	}
	lastFrame = now;
}

static int SCR_PerfSample( int age ) {
	return perfFrameUsec[( perfFrameCount - 1 - age ) & ( PERF_SAMPLES - 1 )];
}

static void SCR_DrawPerfOverlay( void ) {
	static const vec4_t	graphColors[3] = { { 0.2f, 0.9f, 0.2f, 0.8f }, { 0.95f, 0.8f, 0.1f, 0.8f }, { 0.95f, 0.2f, 0.2f, 0.8f } };
	int		samples[PERF_SAMPLES];
	char	text[64];
	int64_t	sum = 0;
	int		n, used = 0;

	n = (int)MIN( perfFrameCount, (unsigned)PERF_SAMPLES );
	if ( n < 2 ) {
		return;
	}

	// average over the last second
	while ( used < n && sum < 1000000 ) {
		sum += SCR_PerfSample( used++ );
	}
	if ( sum <= 0 ) {
		return;
	}

	// 1% low: the frame rate of the 99th percentile frame time
	for ( int i = 0; i < n; i++ ) {
		samples[i] = SCR_PerfSample( i );
	}
	std::nth_element( samples, samples + n * 99 / 100, samples + n );
	const int p99 = MAX( 1, samples[n * 99 / 100] );

	Com_sprintf( text, sizeof( text ), "%4.0f fps  1%% low %4.0f  %6.2f ms",
		used * 1000000.0 / sum, 1000000.0 / p99, sum / 1000.0 / used );

	const int	textWidth = (int)strlen( text ) * con.charWidth;
	const int	textX = cls.glconfig.vidWidth - textWidth - con.charWidth;
	const int	textY = con.charHeight / 2;
	const vec4_t backdrop = { 0.0f, 0.0f, 0.0f, 0.6f };

	re.SetColor( backdrop );
	re.DrawStretchPic( textX - con.charWidth / 2, textY - con.charHeight / 4, textWidth + con.charWidth,
		con.charHeight + con.charHeight / 2, 0, 0, 0, 0, cls.whiteShader, cls.xadjust, cls.yadjust );
	SCR_DrawSmallStringExt( textX, textY, text, g_color_table[ColorIndex(COLOR_WHITE)], qtrue );

	if ( cl_perfOverlay->integer >= 2 ) {
		// one bar per frame, 4 pixels per millisecond, 16.7 and 33.3 ms bands
		const int	height = 4 * 34;
		const int	width = MIN( n, cls.glconfig.vidWidth / 2 );
		const int	x = cls.glconfig.vidWidth - width - con.charWidth;
		const int	y = con.charHeight * 2 + height;

		re.SetColor( backdrop );
		re.DrawStretchPic( x, y - height, width, height, 0, 0, 0, 0, cls.whiteShader, cls.xadjust, cls.yadjust );

		for ( int i = 0; i < width; i++ ) {
			const int usec = SCR_PerfSample( i );
			const int h = MIN( height, usec * 4 / 1000 );

			re.SetColor( graphColors[usec > 33333 ? 2 : usec > 16667 ? 1 : 0] );
			re.DrawStretchPic( x + width - 1 - i, y - h, 1, h, 0, 0, 0, 0, cls.whiteShader, cls.xadjust, cls.yadjust );
		}
		re.SetColor( nullptr );
	}
}

//=============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init( void ) {
	cl_timegraph = Cvar_Get ("timegraph", "0", CVAR_CHEAT);
	cl_debuggraph = Cvar_Get ("debuggraph", "0", CVAR_CHEAT);
	cl_graphheight = Cvar_Get ("graphheight", "32", CVAR_CHEAT);
	cl_graphscale = Cvar_Get ("graphscale", "1", CVAR_CHEAT);
	cl_graphshift = Cvar_Get ("graphshift", "0", CVAR_CHEAT);
	cl_perfOverlay = Cvar_Get ("cl_perfOverlay", "0", CVAR_ARCHIVE | CVAR_GLOBAL);

	scr_initialized = qtrue;
}


//=======================================================

void MV_DrawConnectingInfo( void )
{ // Versioninfo when loading...
	int		 yPos = 5;
	int		 line = 17;
	char	 txtbuf[128];

	Com_sprintf(txtbuf, sizeof(txtbuf), "^1[ ^7JK2MV " JK2MV_VERSION " " PLATFORM_STRING " ^1]");
	SCR_DrawStringExt(320 - SCR_Strlen(txtbuf) * 4, yPos + (line * 0), 8, txtbuf, g_color_table[7], qfalse);

	Com_sprintf(txtbuf, sizeof(txtbuf), "Game-Version^1: ^71.%02d", (int)MV_GetCurrentGameversion());
	SCR_DrawStringExt((int)(320 - SCR_Strlen(txtbuf) * 3.5), yPos + (line * 1), 7, txtbuf, g_color_table[7], qfalse);
}

static qboolean SCR_ShouldSkipBackend(void)
{
	return (qboolean)(com_minimized->integer && !CL_VideoRecording());
}

static qboolean SCR_ShouldDrawDebugGraph(void)
{
	return (qboolean)(cl_debuggraph->integer || cl_timegraph->integer || cl_debugMove->integer);
}

/*
==================
SCR_DrawScreenField

This will be called twice if rendering in stereo mode
==================
*/
void SCR_DrawScreenField( stereoFrame_t stereoFrame ) {
	re.BeginFrame( stereoFrame, SCR_ShouldSkipBackend() );

	if ( !uivm ) {
		Com_DPrintf("draw screen without UI loaded\n");
		return;
	}

	// if the menu is going to cover the entire screen, we
	// don't need to render anything under it
	if (!VM_Call(uivm, UI_IS_FULLSCREEN)) {
		switch( cls.state ) {
		default:
			Com_Error( ERR_FATAL, "SCR_DrawScreenField: bad cls.state" );
			break;
		case CA_CINEMATIC:
			SCR_DrawCinematic();
			break;
		case CA_DISCONNECTED:
			if ( CL_TestSceneActive() ) {
				CL_DrawTestScene();
				break;
			}
			// force menu up
			S_StopAllSounds();
			VM_Call(uivm, UI_SET_ACTIVE_MENU, UIMENU_MAIN);
			break;
		case CA_CONNECTING:
		case CA_CHALLENGING:
		case CA_CONNECTED:
			// workaround for ingame UI not loading connect.menu
			SCR_DrawDefaultConnectBackground();
			// connecting clients will only show the connection dialog
			SCR_DrawConnectScreen(qfalse);
			break;
		case CA_LOADING:
		case CA_PRIMED:
			// draw the game information screen and loading progress
			CL_CGameRendering( stereoFrame );

			MV_DrawConnectingInfo();

			// also draw the connection information, so it doesn't
			// flash away too briefly on local or lan games
			SCR_DrawConnectScreen(qtrue);
			break;
		case CA_ACTIVE:
			CL_CGameRendering( stereoFrame );
			SCR_DrawDemoRecording();
			CL_DrawDemoTimeline();
			break;
		}
	}

	// the menu draws next
	if ( cls.keyCatchers & KEYCATCH_UI && uivm ) {
		SCR_RefreshUI();
	}

	// console draws next
	Con_DrawConsole ();

	// debug graph can be drawn on top of anything
	if ( SCR_ShouldDrawDebugGraph() ) {
		SCR_DrawDebugGraph ();
	}

	if ( cl_perfOverlay->integer ) {
		SCR_DrawPerfOverlay();
	}

	re.EndFrame();
}

static void SCR_DrawStereoFields(void)
{
	if ( cls.glconfig.stereoEnabled ) {
		SCR_DrawScreenField( STEREO_LEFT );
		SCR_DrawScreenField( STEREO_RIGHT );
		return;
	}

	SCR_DrawScreenField( STEREO_CENTER );
}

static void SCR_SwapScreenBuffers(void)
{
	if ( com_speeds->integer ) {
		re.SwapBuffers( &time_frontend, &time_backend );
	} else {
		re.SwapBuffers( nullptr, nullptr );
	}
}

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void SCR_UpdateScreen( void ) {
	static int	recursive;

	if ( !scr_initialized ) {
		return;				// not initialized yet
	}

	if ( ++recursive > 2 ) {
		Com_Error( ERR_FATAL, "SCR_UpdateScreen: recursively called" );
	}
	recursive = 1;

	SCR_RecordFrameTime();

	CL_UpdateRefConfig( );

	SCR_DrawStereoFields();

	CL_TakeVideoFrame();

	SCR_SwapScreenBuffers();

	recursive = 0;
}

#define MAX_SCR_LINES		10
#define SCR_CENTER_WIDTH	76	// 640 / 8 - 4, the original hardcoded line width

int					scr_center_y;

/*
==================
SCR_CenterPrint

Single player center print. Multiplayer never draws it on screen, so only
the console echo is left: the text is word wrapped at SCR_CENTER_WIDTH
characters and at most MAX_SCR_LINES lines are printed.
==================
*/
void SCR_CenterPrint (char *str)//, PalIdx_t colour)
{
	char	*s, *last, *start;
	int		num_chars;
	int		num_lines;
	bool	done = false;
	bool	spaced;

	if (!str)
	{
		return;
	}

	Com_Printf("\n");

	num_lines = 0;
	spaced = false;
	for(s = start = str, last=nullptr, num_chars = 0; !done ; s++)
	{
		num_chars++;
		if ((*s) == ' ')
		{
			spaced = true;
			last = s;
		}

		if ((*s) == '\n' || (*s) == 0)
		{
			last = s;
			num_chars = SCR_CENTER_WIDTH;
			spaced = true;
		}

		if (num_chars >= SCR_CENTER_WIDTH)
		{
			if (!last)
			{
				last = s;
			}
			if (!spaced)
			{
				last++;
			}

			Com_Printf ("%.*s\n", (int)(last - start), start);

			num_lines++;

			// a word hard wrapped right before the terminator also ends the text
			if ((*s) == 0 || (*last) == 0 || num_lines >= MAX_SCR_LINES)
			{
				done = true;
			}
			else
			{
				s = last;
				if (spaced)
				{
					last++;
				}
				start = last;
				last = nullptr;
				num_chars = 0;
				spaced = false;
			}
			continue;
		}
	}

	// echo it to the console
	Com_Printf("\n\n");
	Con_ClearNotify ();
}
