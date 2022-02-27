#include "TrackIterator.h"
#include "csurf.h"

MediaTrack* TrackFromGUID(const GUID& guid) {
	for (TrackIterator ti; !ti.end(); ++ti) {
		MediaTrack* tr = *ti;
		const GUID* tguid = GetTrackGUID(tr);

		if (tr && tguid && !memcmp(tguid, &guid, sizeof(GUID)))
			return tr;
	}
	return NULL;
}



