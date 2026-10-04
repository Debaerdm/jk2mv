// tr_dlightselect.cpp -- which dynamic lights a scene keeps (r_dlightPriority)

#include "tr_dlightselect.h"
#include <algorithm>

// Hysteresis. The cgame gives every saber light a new radius each frame
// (length * 2 + random() * 8, up to 21% apart in radius squared at full
// length), muzzle flashes and powerups too, so lights whose scores are
// close swap places around the cutoff from one frame to the next, and a
// light about its radius behind the viewer goes in and out of the cull. A
// light within DLSEL_SAME_LIGHT units of one kept last time (lights move a
// few units per frame) is taken for that light: its score counts
// DLSEL_KEPT_BONUS times, so another light has to matter that much more to
// replace it, and it is culled only that many radii behind a view.
#define DLSEL_SAME_LIGHT	32.0f
#define DLSEL_KEPT_BONUS	1.5f

static float Dot( const float a[3], const float b[3] ) {
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// the best score from the views, -1 when every view culls the light
static float R_DlightScore( const dlselLight_t *dl, const dlselView_t *views, int numViews, float bonus ) {
	const float	brightness = bonus * dl->brightness * dl->radius * dl->radius;
	float		best = -1.0f;

	for ( int v = 0; v < numViews; v++ ) {
		const float	*origin = views[v].origin;
		const float	delta[3] = { dl->origin[0] - origin[0], dl->origin[1] - origin[1], dl->origin[2] - origin[2] };

		if ( views[v].cullBehind && Dot( delta, views[v].forward ) < -dl->radius * bonus ) {
			continue;
		}
		const float dist2 = std::max( Dot( delta, delta ), 0.25f * dl->radius * dl->radius );
		best = std::max( best, brightness / std::max( dist2, 1.0f ) );
	}
	return best;
}

// flags, for each light kept last time, the light now nearest to it if close
// enough, a different one each time: of two players fighting blade to blade
// only the one that was kept gets the bonus, and two lights at the same spot
// (a muzzle flash and its effect) both keep theirs
static void R_DlightsKeptBefore( const dlselLight_t *lights, int count, const dlselHistory_t *history, bool *wasKept ) {
	for ( int i = 0; i < count; i++ ) {
		wasKept[i] = false;
	}
	for ( int h = 0; history && h < history->count; h++ ) {
		const float	*kept = history->origins[h];
		float		nearestDist2 = DLSEL_SAME_LIGHT * DLSEL_SAME_LIGHT;
		int			nearest = -1;

		for ( int i = 0; i < count; i++ ) {
			const float	delta[3] = { lights[i].origin[0] - kept[0], lights[i].origin[1] - kept[1], lights[i].origin[2] - kept[2] };
			const float	dist2 = Dot( delta, delta );

			if ( !wasKept[i] && dist2 < nearestDist2 ) {
				nearestDist2 = dist2;
				nearest = i;
			}
		}
		if ( nearest >= 0 ) {
			wasKept[nearest] = true;
		}
	}
}

int R_DlightSelect( const dlselLight_t *lights, int count, int keep,
	const dlselView_t *views, int numViews, dlselHistory_t *history, int *chosen ) {
	count = std::min( std::max( count, 0 ), DLSEL_MAX_LIGHTS );
	keep = std::min( std::max( keep, 0 ), DLSEL_MAX_KEEP );
	numViews = std::min( std::max( numViews, 0 ), DLSEL_MAX_VIEWS );

	int numChosen = 0;
	if ( count <= keep ) {
		for ( ; numChosen < count; numChosen++ ) {
			chosen[numChosen] = numChosen;
		}
	} else {
		float	score[DLSEL_MAX_LIGHTS];
		int		order[DLSEL_MAX_LIGHTS];
		bool	wasKept[DLSEL_MAX_LIGHTS];

		R_DlightsKeptBefore( lights, count, history, wasKept );
		for ( int i = 0; i < count; i++ ) {
			order[i] = i;
			score[i] = R_DlightScore( &lights[i], views, numViews, wasKept[i] ? DLSEL_KEPT_BONUS : 1.0f );
		}
		// stable, so lights with equal scores keep the one added first
		std::stable_sort( order, order + count, [&score]( int a, int b ) { return score[a] > score[b]; } );
		std::sort( order, order + keep );	// back to the order they were added in
		for ( ; numChosen < keep; numChosen++ ) {
			chosen[numChosen] = order[numChosen];
		}
	}

	if ( history ) {
		history->count = numChosen;
		for ( int i = 0; i < numChosen; i++ ) {
			const float *kept = lights[chosen[i]].origin;

			history->origins[i][0] = kept[0];
			history->origins[i][1] = kept[1];
			history->origins[i][2] = kept[2];
		}
	}
	return numChosen;
}
