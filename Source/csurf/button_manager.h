#ifndef _BUTTONMANAGER_H
#define _BUTTONMANAGER_H

#pragma once
#include "csurf_faderport.h"

class CSurf_Faderport;

#pragma once
class ButtonManager{
	
	private:
	    CSurf_Faderport* m_pFaderport;
		int m_button_last;
		DWORD m_button_last_time;
	
	public:
	    ButtonManager(CSurf_Faderport * pFaderport);
		bool OnButtonPress(MIDI_event_t* evt);
};

#endif