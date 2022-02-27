
#ifndef _TRACKITERATOR_H_
#define _TRACKITERATOR_H_

#include "csurf.h"

extern MediaTrack* TrackFromGUID(const GUID& guid);

class TrackIterator {
	int m_index;
	int m_len;
public:
	TrackIterator() {
		m_index = 1;
		m_len = CSurf_NumTracks(false);
	}

	MediaTrack* operator*() {
		return CSurf_TrackFromID(m_index, false);
	}

	TrackIterator& operator++() {
		if (m_index <= m_len) ++m_index;
		return *this;
	}

	bool end() {
		return m_index > m_len;
	}

};

struct SelectedTrack {

	SelectedTrack(MediaTrack* tr) {
		this->next = NULL;
		this->guid = *GetTrackGUID(tr);
	}

	MediaTrack* track() {
		return TrackFromGUID(this->guid);
	}

	SelectedTrack* next;
	GUID guid;
};
#endif