#ifndef _FADERPORTVIRTUALSURFACE_H_
#define _FADERPORTVIRTUALSURFACE_H_

#include "csurf.h"

struct VirtualSurface {
	int number_of_tracks;
	int display_number[32];
	int media_track_number[32];
	MediaTrack* media_track[32];
	MediaTrack* media_track_last[32];
};
#endif