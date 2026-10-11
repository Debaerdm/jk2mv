// tr_dlightselect.h -- which dynamic lights a scene keeps when it has more
// than surfaces have bits for (r_dlightPriority)
//
// Plain math with no engine dependency, so the unit tests can link it.

#ifndef TR_DLIGHTSELECT_H
#define TR_DLIGHTSELECT_H

#define DLSEL_MAX_LIGHTS	256		// MAX_DLIGHT_POOL
#define DLSEL_MAX_KEEP		32		// MAX_DLIGHTS
#define DLSEL_MAX_VIEWS		4		// the scene's view and portal cameras

typedef struct {
	float	origin[3];
	float	radius;
	float	brightness;		// largest color component
} dlselLight_t;

// a point the scene is seen from
typedef struct {
	float	origin[3];
	float	forward[3];
	bool	cullBehind;		// nothing else shows what is behind it (no mirror)
} dlselView_t;

// what a view kept last time: lights don't carry an identity, so a light
// close to one of these origins is taken for the same light
typedef struct {
	int		count;
	float	origins[DLSEL_MAX_KEEP][3];
} dlselHistory_t;

// Picks the keep lights that matter most for the views (the first is the
// scene's own): a light entirely behind a view that culls can't reach
// anything visible from it, the others rank by brightness over distance to
// the nearest view, a light around a viewer counting as at half its radius.
// Writes their indexes to chosen in increasing order (the original order)
// and returns how many, all of them when count <= keep.
//
// With a history, a light near one kept last time must be beaten by more
// than its score jitter, or get clearly behind the view, before it is
// dropped, so sabers whose radius changes every frame don't switch on and
// off; the history then holds this choice.
int R_DlightSelect( const dlselLight_t *lights, int count, int keep,
	const dlselView_t *views, int numViews, dlselHistory_t *history, int *chosen );

// The histories of the world scenes of each frame, by the order they are
// drawn in: a second world scene (a mod's camera view) has its own, rather
// than taking the 3D view's choice of the same frame for the last one.
#define DLSEL_MAX_SCENES	4

typedef struct {
	int				frame;		// of the last scene
	int				numScenes;	// scenes of that frame so far
	dlselHistory_t	scenes[DLSEL_MAX_SCENES];
} dlselSceneHistories_t;

// The history of the next world scene of frame, NULL past DLSEL_MAX_SCENES
// in a frame (no hysteresis for those).
dlselHistory_t *R_DlightSceneHistory( dlselSceneHistories_t *histories, int frame );

// The history a scene of frame selects with: none without priority
// (r_dlightPriority 0) or without a world (menus, HUD models: they neither
// use a world scene's history nor take its place in the frame), else the
// next world scene's. frame must be the same for every scene of a frame and
// change from one frame to the next.
dlselHistory_t *R_DlightHistoryForScene( dlselSceneHistories_t *histories, bool priority, bool world, int frame );

#endif
