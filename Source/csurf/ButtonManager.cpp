#include "button_manager.h"
#include "faderport_buttons.h"
#include "csurf_faderport.h"

ButtonManager::ButtonManager(CSurf_Faderport* pFaderport) : m_pFaderport(pFaderport)
{
	m_button_last = 0x0;
	m_button_last_time = 0;
}

typedef bool (CSurf_Faderport::* MidiHandlerFunc)(MIDI_event_t*);

struct btnHandler {
	unsigned int evt_min;
	unsigned int evt_max; // inclusive
	MidiHandlerFunc func;
	MidiHandlerFunc func_dc;
	MidiHandlerFunc func_shift;
};


bool ButtonManager::OnButtonPress(MIDI_event_t * evt) {
	if ((evt->midi_message[0] & 0xf0) != 0x90)
		return false;

	static const int nHandlers = 3;
	static const int nPressOnlyHandlers = 3;
	static const btnHandler handlers[nHandlers] = {
		//	{ B_ARM, B_ARM,			  	 &CSurf_Faderport::OnRecArm,        &CSurf_Faderport::OnRecArmDC },
			//{ B_ARM, B_ARM,			  	 &CSurf_Faderport::OnArm,        &CSurf_Faderport::OnRecArmDC },

			//{ B_SOLO_CLEAR, B_SOLO_CLEAR,&CSurf_Faderport::OnSoloClear },
			//{ B_MUTE_CLEAR, B_MUTE_CLEAR,&CSurf_Faderport::OnMuteClear },

			//{ B_SELECT1, B_SELECT8,		 &CSurf_Faderport::OnChannelSelect, &CSurf_Faderport::OnChannelSelectDC },
			//{ B_SELECT9, B_SELECT9,		 &CSurf_Faderport::OnChannelSelect, &CSurf_Faderport::OnChannelSelectDC },
			//{ B_SELECT10, B_SELECT16,	 &CSurf_Faderport::OnChannelSelect, &CSurf_Faderport::OnChannelSelectDC },

			//{ B_MUTE1, B_MUTE8,			 &CSurf_Faderport::OnMute },
			//{ B_MUTE9, B_MUTE16,	     &CSurf_Faderport::OnMute },
			//{ B_SOLO1, B_SOLO8,			 &CSurf_Faderport::OnSolo,			&CSurf_Faderport::OnSoloDC },
			//{ B_SOLO9, B_SOLO11,		 &CSurf_Faderport::OnSolo,			&CSurf_Faderport::OnSoloDC },
			//{ B_SOLO12, B_SOLO12,		 &CSurf_Faderport::OnSolo,			&CSurf_Faderport::OnSoloDC },
			//{ B_SOLO13, B_SOLO14,		 &CSurf_Faderport::OnSolo,			&CSurf_Faderport::OnSoloDC },
			//{ B_SOLO15, B_SOLO15,		 &CSurf_Faderport::OnSolo,			&CSurf_Faderport::OnSoloDC },
			//{ B_SOLO16, B_SOLO16,		 &CSurf_Faderport::OnSolo,			&CSurf_Faderport::OnSoloDC },

			//// Press down only events
			//{ B_AUTO_LATCH, B_SAVE,		 &CSurf_Faderport::OnAutoMode,		NULL, &CSurf_Faderport::OnSave },
			//{ B_AUTO_TRIM, B_REDO,		 &CSurf_Faderport::OnAutoMode,		NULL, &CSurf_Faderport::OnRedo },
			//{ B_AUTO_OFF, B_AUTO_OFF,	 &CSurf_Faderport::OnAutoMode,		NULL, &CSurf_Faderport::OnUndo },
			//{ B_AUTO_TOUCH, B_AUTO_TOUCH,&CSurf_Faderport::OnAutoMode,		NULL, &CSurf_Faderport::OnUser },
			//{ B_AUTO_WRITE, B_AUTO_WRITE,&CSurf_Faderport::OnAutoMode,		NULL, &CSurf_Faderport::OnUser },
			//{ B_AUTO_READ, B_AUTO_READ,  &CSurf_Faderport::OnAutoMode,		NULL, &CSurf_Faderport::OnUser },

			//{ B_ENCODER_PUSH, B_ENCODER_PUSH, &CSurf_Faderport::OnRotaryEncoderPush },
			//{ B_PREV, B_NEXT,			 &CSurf_Faderport::OnPrevNext },

			//{ B_CHANNEL, B_CHANNEL,		 &CSurf_Faderport::OnChannel,       NULL, &CSurf_Faderport::OnFunctionKey },
			//{ B_ZOOM, B_ZOOM,			 &CSurf_Faderport::OnZoom,		    NULL, &CSurf_Faderport::OnFunctionKey },
			//{ B_SCROLL, B_SCROLL,		 &CSurf_Faderport::OnScroll,	    NULL, &CSurf_Faderport::OnFunctionKey },
			//{ B_BANK, B_BANK,			 &CSurf_Faderport::OnBank,		    NULL, &CSurf_Faderport::OnFunctionKey },
			//{ B_MASTER, B_MASTER,		 &CSurf_Faderport::OnMaster,		NULL, &CSurf_Faderport::OnFunctionKey },
			//{ B_CLICK, B_CLICK,			 &CSurf_Faderport::OnClick,         NULL, &CSurf_Faderport::OnFunctionKey },
			//{ B_SECTION, B_SECTION,	     &CSurf_Faderport::OnSection, NULL, &CSurf_Faderport::OnFunctionKey },
			//{ B_MARKER, B_MARKER,		 &CSurf_Faderport::OnMarker, NULL, &CSurf_Faderport::OnFunctionKey },


			//{ B_CYCLE, B_CYCLE,			 &CSurf_Faderport::OnTransport },
			//{ B_RR, B_RR,				 &CSurf_Faderport::OnTransport },
			//{ B_FF, B_FF,				 &CSurf_Faderport::OnTransport },
			//{ B_STOP, B_STOP,			 &CSurf_Faderport::OnTransport },
			//{ B_PLAY, B_PLAY,			 &CSurf_Faderport::OnTransport },
			//{ B_RECORD, B_RECORD,		 &CSurf_Faderport::OnTransport },

			//{ B_SHIFTL ,B_SHIFTL,		 &CSurf_Faderport::OnShift },
			//{ B_SHIFTR ,B_SHIFTR,		 &CSurf_Faderport::OnShift },

			//{ B_PAN, B_PAN,				 &CSurf_Faderport::OnPan },
			//{ B_ALL, B_USER,			 &CSurf_Faderport::OnAll },

			//// Press and release events
			////{ 0x46, 0x49, &CSurf_Faderport::OnKeyModifier },
			//{ 0x68, 0x70, &CSurf_Faderport::OnTouch },
	};

	unsigned int evt_code = evt->midi_message[1];  //get_midi_evt_code( evt );

	
	// For these events we only want to track button press
	if (evt->midi_message[2] >= 0x40) {

		// Check for double click
		DWORD now = timeGetTime();
		bool double_click = (int)evt_code == m_button_last && now - m_button_last_time < DOUBLE_CLICK_INTERVAL;
		m_button_last = evt_code;
		m_button_last_time = now;

		// Find event handler
		for (int i = 0; i < nPressOnlyHandlers; i++) {
			btnHandler bh = handlers[i];

			if (bh.evt_min <= evt_code && evt_code <= bh.evt_max) {

				// Test for shift
				if (!m_pFaderport->m_shift_state || bh.func_shift == NULL || !(m_pFaderport->*bh.func_shift)(evt))
					return true;

				// Test double-click
				if (double_click && bh.func_dc != NULL && (m_pFaderport->*bh.func_dc)(evt))
					return true;

				// Single click (and unhandled double clicks)
				if (bh.func != NULL && (m_pFaderport->*bh.func)(evt))
					return true;
			}
		}
	}

	// For these events we want press and release
	for (int i = nPressOnlyHandlers; i < nHandlers; i++)
		if (handlers[i].evt_min <= evt_code && evt_code <= handlers[i].evt_max)
			if ((m_pFaderport->*handlers[i].func)(evt)) return true;

	// Pass thru if not otherwise handled
	if (evt->midi_message[2] >= 0x40) {
		int a = evt->midi_message[1];
		//MIDI_event_t evt = { 0,3,{0xbf - (m_mackie_modifiers & 15),a,0} };
		MIDI_event_t evt = { 0,3,{0xbf ,a,0} };
		kbd_OnMidiEvent(&evt, -1);
	}

	return true;
}
