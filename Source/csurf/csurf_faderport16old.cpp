/*
** reaper_csurf
** MCU support
** Copyright (C) 2006-2008 Cockos Incorporated
** License: LGPL.
*/
#include "csurf.h"
#include "TrackIterator.h"
#include "WDL/ptrlist.h"
#include "converters.h"
#include "faderport_buttons.h"
#include "faderport_codes.h"
#include "dialoghandler.h"

class CSurf_Faderport;
static WDL_PtrList<CSurf_Faderport> m_mcu_list;

static bool g_csurf_mcpmode; // follow mixer
static int m_flipmode;
static int m_allmcus_bank_offset;

typedef void (CSurf_Faderport::* ScheduleFunc)();

struct ScheduledAction {
	ScheduledAction(DWORD time, ScheduleFunc func) {
		this->next = NULL;
		this->time = time;
		this->func = func;
	}

	ScheduledAction* next;
	DWORD time;
	ScheduleFunc func;
};

class CSurf_Faderport : public IReaperControlSurface
{
	
	midi_Output* m_midiout;
	midi_Input* m_midiin;

	bool m_reset = false;
	int m_midi_in_dev, m_midi_out_dev;
	int m_offset, m_size;
	bool m_is_fp8;
	int m_metronom_offset;
	int m_vol_lastpos[256];
	int m_pan_lastpos[256];
	char m_mackie_lasttime[10];
	int m_mackie_lasttime_mode;
	int m_mackie_modifiers;
	int m_cfg_flags;  //CONFIG_FLAG_FADER_TOUCH_MODE etc

	// create a state structure instead
	bool m_cycle_state;
	bool m_shift_state;
	bool m_channel_state;
	bool m_bank_state;
	bool m_master_state;
	bool m_section_state;
	bool m_marker_state;
	bool m_zoom_state;
	bool m_scroll_state;
	bool m_arm_state;

	char m_fader_touchstate[256];
	unsigned int m_fader_lasttouch[256]; // m_fader_touchstate changes will clear this, moves otherwise set it. if set to -1, then totally disabled
	unsigned int m_pan_lasttouch[256];

	WDL_String m_descspace;
	char m_configtmp[1024];

	double m_mcu_meterpos[16];
	DWORD m_mcu_timedisp_lastforce, m_mcu_meter_lastrun;
	int m_mackie_arrow_states;
	unsigned int m_buttonstate_lastrun;
	unsigned int m_frameupd_lastrun;
	ScheduledAction* m_schedule;
	SelectedTrack* m_selected_tracks;

	// If user accidentally hits fader, we want to wait for user
	// to stop moving fader and then reset it to it's orginal position
	
	#define FADER_REPOS_WAIT 250
	bool m_repos_faders;
	DWORD m_fader_lastmove;

	int m_button_last;
	DWORD m_button_last_time;

#pragma region Init
	void ScheduleAction(DWORD time, ScheduleFunc func) {
		ScheduledAction* action = new ScheduledAction(time, func);
		if (m_schedule == NULL) {
			m_schedule = action;

			return;
		}

		if (action->time < m_schedule->time) {
			action->next = m_schedule;
			m_schedule = action;

			return;
		}

		ScheduledAction* curr = m_schedule;
		while (curr->next != NULL && curr->next->time < action->time)
			curr = curr->next;

		action->next = curr->next;
		curr->next = action;
	}
	
	void SetArmedBtnColor() {
		for (int x = 0; x < m_size; x++) {
			SetBtnColour(x, 0xFF0000);
		}
	}

	void ResetAll() {

		if (m_reset) return;
		m_reset = true;
		ResetStates();
		ResetBtns();

		for (int strip_id = 0; strip_id < m_size; strip_id++) {
			ResetFader(strip_id);
			ClearScribbleText(strip_id);
			ResetValueBar(strip_id, MODE_OFF);
			ResetScribble(strip_id);
			ClearSoloBtn(strip_id);
			ClearMuteBtn(strip_id);
			ClearMeter(strip_id);
		  	ClearSelect(strip_id);

			Sleep(5);
		}
	}

	int stripIdToSelectBtn(int id) {
		if (!m_is_fp8 && id == 8) {
			return B_SELECT9;
		}

		int btn_id = 0;

		(!m_is_fp8 && id > 7) ? btn_id = B_SELECT10 - 1 + (id & 7) : btn_id = B_SELECT1 + id;

		return btn_id;
	}

	void ClearSelect(int id) {
		if (!m_is_fp8 && id == 8) {
			m_midiout->Send(BTN, B_SELECT9, STATE_RELEASED, -1);
			return;
		}

		(!m_is_fp8 && id > 7) ? m_midiout->Send(BTN, B_SELECT10 - 1 + (id & 7), STATE_RELEASED, -1) : m_midiout->Send(BTN, B_SELECT1 + id, STATE_RELEASED, -1);
	}

	void ClearSelectBtns() {
		for (int i = 0; i < m_size; i++) {
			ClearSelect(i);
		}
	}

	void ClearShift() {
		m_shift_state = false;
		m_midiout->Send(BTN, B_SHIFTL, STATE_RELEASED, 0);
		m_midiout->Send(BTN, B_SHIFTR, STATE_RELEASED, 0);
	}

	void ResetFader(int id) {
		m_midiout->Send(FADER + id, 0x40 + 10, STATE_RELEASED, 0);
	}

	void ClearSoloBtn(int id) {
		(!m_is_fp8 && id > 7) ? m_midiout->Send(BTN, B_SOLO9 + (id & 7), STATE_RELEASED, -1) : m_midiout->Send(BTN, B_SOLO1 + id, STATE_RELEASED, -1);
	}

	void ClearMuteBtn(int id) {
		(!m_is_fp8 && id > 7) ? m_midiout->Send(BTN, B_MUTE9 + (id & 7), STATE_RELEASED, -1) : m_midiout->Send(BTN, B_MUTE1 + id, STATE_RELEASED, -1);
	}

	void ClearMeter(int id) {
		id = id & 7;
		(!m_is_fp8 && id > 7) ? m_midiout->Send(PEAK_METER_9_16 + (id & 7), STATE_RELEASED, 0, -1) : m_midiout->Send(PEAK_METER_1_8 + id, STATE_RELEASED, 0, -1);
	}

	void ClearScribbleText(int id) {
		char buf[7] = { 0, };
		SendTextToScribble(id, buf);
	}

	void ResetValueBar(int id, int mode) {
		// Set valuebar style
		(!m_is_fp8 && id > 7) ? m_midiout->Send(VALUEBAR, VALUEBAR_MODE_OFFSET_9_16 + (id & 7), mode, -1) : m_midiout->Send(VALUEBAR, VALUEBAR_MODE_OFFSET_1_8 + id, mode, -1);

		// pan centre
		(!m_is_fp8 && id > 7) ? m_midiout->Send(VALUEBAR, VALUEBAR_VALUE_9_16 + (id & 7), 0x40, -1) : m_midiout->Send(VALUEBAR, VALUEBAR_VALUE_1_8 + id, 0x40, -1);
	}

	void ResetBtns() {
		m_midiout->Send(BTN, B_TRACK, STATE_PRESSED, -1);
		m_midiout->Send(BTN, B_EDITPLUGINS, STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_SENDS, STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_PAN, STATE_RELEASED, -1);

		m_midiout->Send(BTN, B_AUDIO, STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_VI, STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_BUS, STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_VCA, STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_ALL, !g_csurf_mcpmode ? STATE_PRESSED : STATE_RELEASED, -1);

		m_midiout->Send(BTN, B_CHANNEL, m_channel_state ? STATE_PRESSED : STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_BANK, m_bank_state ? STATE_PRESSED : STATE_RELEASED, -1);

		m_midiout->Send(BTN, B_MUTE_CLEAR, STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_SOLO_CLEAR, STATE_RELEASED, -1);

		m_midiout->Send(BTN, B_SHIFTL, STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_SHIFTR, STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_SOLO12, STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_SOLO16, STATE_RELEASED, -1);

		m_midiout->Send(BTN, B_AUTO_OFF, STATE_RELEASED, -1);

		m_midiout->Send(BTN, B_AUTO_LATCH, STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_AUTO_TRIM, STATE_PRESSED, -1);
		m_midiout->Send(BTN, B_AUTO_TOUCH, STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_AUTO_WRITE, STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_AUTO_READ, STATE_RELEASED, -1);

		m_midiout->Send(BTN, B_ZOOM, STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_SCROLL, STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_MASTER, STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_SECTION, STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_MARKER, STATE_RELEASED, -1);

		m_midiout->Send(BTN, B_ARM, STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_BYPASS, STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_MACRO, STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_LINK, STATE_RELEASED, -1);

	}

	void ResetStates() {
		m_shift_state = false;
		m_channel_state = false;
		m_bank_state = true;
		g_csurf_mcpmode = false;
		m_section_state = false;
		m_marker_state = false;
		m_master_state = false;
		m_zoom_state = false;
		m_scroll_state = false;
		m_arm_state = false;
	}

	void ResetScribble(int id) {
		struct
		{
			MIDI_event_t evt;
			char data[9];
		}
		poo;
		poo.evt.frame_offset = 0;
		poo.evt.size = 9;
		poo.evt.midi_message[0] = 0xF0;
		poo.evt.midi_message[1] = 0x00;
		poo.evt.midi_message[2] = 0x01;
		poo.evt.midi_message[3] = 0x06;
		poo.evt.midi_message[4] = m_is_fp8 ? FP8 : FP16;

		poo.evt.midi_message[5] = SCRIBBLE;
		poo.evt.midi_message[6] = 0x00 + id;			
		poo.evt.midi_message[7] = SCRIBBLE_DEFAULT_TEXT_METERING;
		poo.evt.midi_message[8] = 0xF7;

		m_midiout->SendMsg(&poo.evt, -1);
	}

	void FaderportReset()
	{
		if (m_midiout ==NULL) return;

		memset(m_mackie_lasttime, 0, sizeof(m_mackie_lasttime));
		memset(m_fader_touchstate, 0, sizeof(m_fader_touchstate));
		memset(m_fader_lasttouch, 0, sizeof(m_fader_lasttouch));
		memset(m_pan_lasttouch, 0, sizeof(m_pan_lasttouch));
		m_mackie_lasttime_mode = -1;
		m_mackie_modifiers = 0;
		m_buttonstate_lastrun = 0;
		m_mackie_arrow_states = 0;

		memset(m_vol_lastpos, 0xff, sizeof(m_vol_lastpos));
		memset(m_pan_lastpos, 0xff, sizeof(m_pan_lastpos));

		// Get metronome status
		int sz;
		m_metronom_offset = projectconfig_var_getoffs("projmetroen", &sz); 
	
		if (sz != 4)
			m_metronom_offset = 0;
		
		ResetAll();

		SetCycleState();

	}

	void SendTextToScribble(int scribble_id, const char* text)
	{
	
		if (m_midiout == NULL) return;

		struct
		{
			MIDI_event_t evt;
			char data[512];
		}
		poo;

		poo.evt.frame_offset = 0;
		poo.evt.size = 0;
		poo.evt.midi_message[poo.evt.size++] = 0xF0;
		poo.evt.midi_message[poo.evt.size++] = 0x00;
		poo.evt.midi_message[poo.evt.size++] = 0x01;
		poo.evt.midi_message[poo.evt.size++] = 0x06;
		poo.evt.midi_message[poo.evt.size++] = m_is_fp8 ? FP8 : FP16;

		//<SysExHdr> 12, xx, yy, zz, tx,tx,tx,... F7
		poo.evt.midi_message[poo.evt.size++] = SCRIBBLE_SEND_STRING; 
		poo.evt.midi_message[poo.evt.size++] = scribble_id;				// xx strip id
		poo.evt.midi_message[poo.evt.size++] = 0x00;					// yy line number 0-3 
		poo.evt.midi_message[poo.evt.size++] = 0x0000000;				// zz alignment flag 0000000=centre

		int length = strlen(text);

		if (length > 200) length = 200;

		int cnt = 0;

		while (cnt < length)
		{
			poo.evt.midi_message[poo.evt.size++] = *text++;				// tx text in ASCII format
			cnt++;
		}

		poo.evt.midi_message[poo.evt.size++] = END;
		m_midiout->SendMsg(&poo.evt, -1);
	}

	typedef bool (CSurf_Faderport::* MidiHandlerFunc)(MIDI_event_t*);

#pragma endregion Init
	
#pragma region Faderport_Events
	bool OnFaderportReset(MIDI_event_t* evt) {
		
		//if (evt->midi_message[0] == 0xf0)
		//{
		//	// on reset
		//	FaderportReset();
		//	TrackList_UpdateAllExternalSurfaces();
		//	return true;
		//}

		return true;
	}

	bool OnFaderMove(MIDI_event_t* evt) {

		if ((evt->midi_message[0] & 0xf0) != FADER) return false;

		m_fader_lastmove = timeGetTime();

		int track_id = evt->midi_message[0] & 0xf;

		if (track_id >= 0 && track_id <= m_size && m_fader_lasttouch[track_id] != 0xffffffff)
			m_fader_lasttouch[track_id] = m_fader_lastmove;

		track_id += 1 + m_allmcus_bank_offset;

		MediaTrack* track = CSurf_TrackFromID(track_id, g_csurf_mcpmode);

		if (track == NULL) return false;

		if ((m_cfg_flags & CONFIG_FLAG_FADER_TOUCH_MODE) && !GetTouchState(track)) {
			m_repos_faders = true;
			return true;
		}

		if (m_flipmode)
		{
			CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, int14ToPan(evt->midi_message[2], evt->midi_message[1]), false), NULL);
			return true;
		}

		CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, int14ToVol(evt->midi_message[2], evt->midi_message[1]), false), NULL);

		return true;
	}

	bool OnRotaryEncoder(MIDI_event_t * evt) {
		if ((evt->midi_message[0] & 0xf0) == 0xb0 && evt->midi_message[1] >= 0x10 && evt->midi_message[1] < 0x18) // pan
		{
			int track_id = evt->midi_message[1] - 0x10;

			m_pan_lasttouch[track_id] = timeGetTime();
			track_id += 1 + m_allmcus_bank_offset;
			MediaTrack* track = CSurf_TrackFromID(track_id, g_csurf_mcpmode);

			if (track == NULL) return false;

			double adj = (evt->midi_message[2] & 0x3f) / 31.0;

			if (evt->midi_message[2] & 0x40) adj = -adj;

			if (m_flipmode)
			{
				CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, adj * 11.0, true), NULL);
				return true;
			}

			CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, adj, true), NULL);
			return true;

		}

		return false;
	}

	bool OnAutoMode(MIDI_event_t * evt) {
		int mode = -1;
		int a = evt->midi_message[1] - 0x4a;

		if (!a) mode = AUTO_MODE_READ;

		if (a == 1) mode = AUTO_MODE_WRITE;
		if (a == 2) mode = AUTO_MODE_TRIM;
		if (a == 3) mode = AUTO_MODE_TOUCH;
		if (a == 4) mode = AUTO_MODE_LATCH;

		if (mode >= 0)
			SetAutomationMode(mode, !IsKeyDown(VK_CONTROL));

		return true;
	}

	bool OnJogWheel(MIDI_event_t * evt) {

		if ((evt->midi_message[0] & 0xf0) != ENCODER || evt->midi_message[1] != JOG_WHEEL) return false;
	
			// improve this!
			if (m_master_state) {
				MediaTrack* tr = CSurf_TrackFromID(0, g_csurf_mcpmode);
				double adj = (evt->midi_message[2] & 0x3f) / 31.0;
				if (evt->midi_message[2] & 0x40) adj = -adj;
				CSurf_SetSurfaceVolume(tr, CSurf_OnVolumeChange(tr, adj * 15.0, true), NULL);

				return true;
			}

			// add a modifier for smaller steps
			if (evt->midi_message[2] == 0x41)
				CSurf_OnRew(500);

			if (evt->midi_message[2] == 0x01)
				CSurf_OnFwd(500);

			return true;
	}

	bool OnPrevNext(MIDI_event_t * evt) {
		int maxfaderpos = 0;
		int movesize;

		m_is_fp8 ? movesize = 8 : movesize = 16;

		// handle multiple surfaces?
		//int x;
		//for (x = 0; x < m_mcu_list.GetSize(); x++)
		//{
		//	CSurf_Faderport* item = m_mcu_list.Get(x);
		//	if (item)
		//	{
		//		if (item->m_offset + 8 > maxfaderpos)
		//			maxfaderpos = item->m_offset + 8;
		//	}
		//}

		// Fix this depending on how multiple surfaces should work
		maxfaderpos = m_offset + movesize;

		if (m_channel_state) movesize = 1;

		if (evt->midi_message[1] & 1) // increase by X
		{
			int msize = CSurf_NumTracks(g_csurf_mcpmode);

			if (movesize > 1 && m_allmcus_bank_offset + maxfaderpos >= msize) return true;

			m_allmcus_bank_offset += movesize;

			if (m_allmcus_bank_offset >= msize) m_allmcus_bank_offset = msize - 1;
		}
		else
		{
			m_allmcus_bank_offset -= movesize;

			if (m_allmcus_bank_offset < 0) m_allmcus_bank_offset = 0;
		}

		// update all of the sliders
		TrackList_UpdateAllExternalSurfaces();

		return true;
	}

	bool OnBank(MIDI_event_t* evt) {
		if (evt->midi_message[1] != B_BANK) return false;

		// Turn off button if already on
		if (m_channel_state) {
			m_midiout->Send(BTN, B_BANK, STATE_RELEASED, -1);
			m_channel_state = false;
			return true;
		}

		// light the button and turn off the corresponding button
		m_channel_state = true;
		m_bank_state = false;

		m_midiout->Send(BTN, B_BANK, STATE_PRESSED, -1);
		m_midiout->Send(BTN, B_CHANNEL, STATE_RELEASED, -1);

		return true;
	}

	bool OnChannel(MIDI_event_t* evt) {
		if (evt->midi_message[1] != B_CHANNEL) return false;

		//// Turn off button if already on
		if (m_bank_state) {
			m_midiout->Send(BTN, B_CHANNEL, STATE_RELEASED, -1);
			m_bank_state = false;
			return true;
		}

		// light the button and turn off the corresponding button
		m_bank_state = true;
		m_channel_state = false;

		m_midiout->Send(BTN, B_CHANNEL, STATE_PRESSED, -1);
		m_midiout->Send(BTN, B_BANK, STATE_RELEASED, -1);

		return true;
	}

	bool OnZoom(MIDI_event_t* evt) {
		if (evt->midi_message[1] != B_ZOOM) return false;

		// Turn off button if already on
		if (m_zoom_state) {
			m_midiout->Send(BTN, B_ZOOM, STATE_RELEASED, -1);
			m_zoom_state = false;
			return true;
		}

		// light the button and turn off the corresponding button
		m_zoom_state = true;
		m_scroll_state = false;

		m_midiout->Send(BTN, B_ZOOM, STATE_PRESSED, -1);
		m_midiout->Send(BTN, B_SCROLL, STATE_RELEASED, -1);

		//SendMessage(g_hwnd, WM_COMMAND, IsKeyDown(VK_SHIFT) ? ID_INSERT_MARKERRGN : ID_INSERT_MARKER, 0);
		return true;
	}

	bool OnScroll(MIDI_event_t* evt) {
		if (evt->midi_message[1] != B_SCROLL) return false;

		//// Turn off button if already on
		if (m_scroll_state) {
			m_midiout->Send(BTN, B_SCROLL, STATE_RELEASED, -1);
			m_scroll_state = false;
			return true;
		}

		// light the button and turn off the corresponding button
		m_scroll_state = true;
		m_zoom_state = false;

		m_midiout->Send(BTN, B_SCROLL, STATE_PRESSED, -1);
		m_midiout->Send(BTN, B_ZOOM, STATE_RELEASED, -1);

		//SendMessage(g_hwnd, WM_COMMAND, IsKeyDown(VK_SHIFT) ? ID_INSERT_MARKERRGN : ID_INSERT_MARKER, 0);
		return true;
	}

	bool OnMaster(MIDI_event_t * evt) {
		m_master_state = !m_master_state;

		m_midiout->Send(BTN, B_MASTER, m_master_state ? STATE_PRESSED : STATE_RELEASED, -1);

		return true;
	}

	bool OnClick(MIDI_event_t * evt) {
		SendMessage(g_hwnd, WM_COMMAND, ID_METRONOME, 0);
		UpdateMetronomLED();

		return true;
	}

	bool OnMarker(MIDI_event_t* evt) {
		if(evt->midi_message[1] != B_MARKER) return false;

		// Turn off button if already on
		if(m_marker_state) {
			m_midiout->Send(BTN, B_MARKER, STATE_RELEASED, -1);
			m_marker_state = false;
			return true;
		}

		// light the button and turn off the corresponding button
		m_marker_state = true;
		m_section_state = false;

		m_midiout->Send(BTN, B_MARKER, STATE_PRESSED, -1);
		m_midiout->Send(BTN, B_SECTION, STATE_RELEASED, -1);

		//SendMessage(g_hwnd, WM_COMMAND, IsKeyDown(VK_SHIFT) ? ID_INSERT_MARKERRGN : ID_INSERT_MARKER, 0);
		return true;
	}

	bool OnSection(MIDI_event_t* evt) {
		if (evt->midi_message[1] != B_SECTION) return false;

		//// Turn off button if already on
		if (m_section_state) {
			m_midiout->Send(BTN, B_SECTION, STATE_RELEASED, -1);
			m_section_state = false;
			return true;
		}

		// light the button and turn off the corresponding button
		m_section_state = true;
		m_marker_state = false;

		m_midiout->Send(BTN, B_SECTION, STATE_PRESSED, -1);
		m_midiout->Send(BTN, B_MARKER, STATE_RELEASED, -1);

		//SendMessage(g_hwnd, WM_COMMAND, IsKeyDown(VK_SHIFT) ? ID_INSERT_MARKERRGN : ID_INSERT_MARKER, 0);
		return true;
	}

	bool OnMute(MIDI_event_t * evt) {
		int mute_id = evt->midi_message[1];

		int track_id = mute_id - B_MUTE1;

		if (!m_is_fp8 && mute_id >= B_MUTE9) track_id = mute_id - 0x70;

		track_id += 1 + m_allmcus_bank_offset;

		MediaTrack* track = CSurf_TrackFromID(track_id, g_csurf_mcpmode);

		if (track)
		{
			CSurf_SetSurfaceMute(track, CSurf_OnMuteChange(track, -1), NULL);
		}

		return true;
	}

	bool OnSolo(MIDI_event_t * evt) {
		int soloId = evt->midi_message[1];

		int track_id = soloId - B_SOLO1; // solo 1 button starts at 0x08

		if (!m_is_fp8 && soloId >= B_SOLO9 && soloId <= B_SOLO14) track_id = soloId - 0x48; // solo 9 button starts at 0x50

		if (!m_is_fp8 && soloId == B_SOLO12) track_id = 11; // solo 12

		if (!m_is_fp8 && soloId == B_SOLO15) track_id = 14; // solo 15

		if (!m_is_fp8 && soloId == B_SOLO16) track_id = 15; // solo 16

		track_id += 1 + m_allmcus_bank_offset;

		MediaTrack* track = CSurf_TrackFromID(track_id, g_csurf_mcpmode);

		if (track)
		{
			CSurf_SetSurfaceSolo(track, CSurf_OnSoloChange(track, -1), NULL);
		}

		return true;
	}

	// double click - select only dc'd track
	bool OnSoloDC(MIDI_event_t * evt) {
		int track_id = evt->midi_message[1] - 0x08;

		track_id += 1 + m_allmcus_bank_offset + m_offset;
		MediaTrack* track = CSurf_TrackFromID(track_id, g_csurf_mcpmode);
		SoloAllTracks(0);
		CSurf_SetSurfaceSolo(track, CSurf_OnSoloChange(track, 1), NULL);

		return true;
	}

	bool OnSoloClear(MIDI_event_t * evt) {
		int muteId = evt->midi_message[1];

		m_midiout->Send(BTN, B_SOLO_CLEAR, 0, -1);
		SoloAllTracks(0);

		return true;
	}

	bool OnMuteClear(MIDI_event_t * evt) {
		int muteId = evt->midi_message[1];

		m_midiout->Send(BTN, B_MUTE_CLEAR, 0, -1);
		MuteAllTracks(false);

		return true;
	}

	// Select each track
	bool OnChannelSelect(MIDI_event_t * evt) {

		if (m_arm_state) {
			OnRecArm(evt);
			//return true;
		}

		//convert the button number to a track id
		int track_id = evt->midi_message[1] - 0x18;

		// Map button 9 correctly
		if (evt->midi_message[1] == B_SELECT9)
			track_id = 8;

		track_id += 1 + m_allmcus_bank_offset + m_offset;;

		MediaTrack* track = CSurf_TrackFromID(track_id, g_csurf_mcpmode);

		if (track) CSurf_OnSelectedChange(track, -1); // this will automatically update the surface

		return true;
	}

	// Doubleclick event
	// Select only this track
	bool OnChannelSelectDC(MIDI_event_t * evt) {
		int track_id = evt->midi_message[1] - 0x18;

		// Map button 9 correctly
		if (evt->midi_message[1] == B_SELECT9)
			track_id = 8;

		track_id += 1 + m_allmcus_bank_offset + m_offset;

		MediaTrack* track = CSurf_TrackFromID(track_id, g_csurf_mcpmode);

		// Clear already selected tracks
		SelectedTrack* i = m_selected_tracks;

		while (i) {
			// Call to OnSelectedChange will cause 'i' to be destroyed, so go ahead
			// and get 'next' now
			SelectedTrack* next = i->next;
			MediaTrack* track = i->track();
			if (track) CSurf_OnSelectedChange(track, 0);

			i = next;
		}

		// Select this track
		CSurf_OnSelectedChange(track, 1);

		return true;
	}

	bool OnTransport(MIDI_event_t * evt) {
		switch (evt->midi_message[1]) {
		case B_RECORD:
			CSurf_OnRecord();
			break;
		case B_PLAY:
			CSurf_OnPlay();
			break;
		case B_STOP:
			CSurf_OnStop();
			break;
		case B_RR:
			SendMessage(g_hwnd, WM_COMMAND, ID_MARKER_PREV, 0);
			break;
		case B_FF:
			SendMessage(g_hwnd, WM_COMMAND, ID_MARKER_NEXT, 0);
			break;
		case B_CYCLE:
			SendMessage(g_hwnd, WM_COMMAND, IDC_REPEAT, 0);
			break;
		case B_CLICK:
			SendMessage(g_hwnd, WM_COMMAND, ID_METRONOME, 0);
		}

		return true;
	}

	bool OnSave(MIDI_event_t * evt) {
		m_midiout->Send(BTN, B_SAVE, STATE_PRESSED, -1);
		SendMessage(g_hwnd, WM_COMMAND, IsKeyDown(VK_SHIFT) ? ID_FILE_SAVEAS : ID_FILE_SAVEPROJECT, 0);
		ScheduleAction(timeGetTime() + 1000, &CSurf_Faderport::ClearSaveLed);
		ClearShift();

		return true;
	}

	bool OnRedo(MIDI_event_t * evt) {
		m_midiout->Send(BTN, B_REDO, STATE_PRESSED, -1);
		SendMessage(g_hwnd, WM_COMMAND, IDC_EDIT_REDO, 0);
		ScheduleAction(timeGetTime() + 1000, &CSurf_Faderport::ClearSaveLed);
		ClearShift();

		return true;
	}

	bool OnUndo(MIDI_event_t * evt) {
		m_midiout->Send(BTN, B_UNDO, STATE_PRESSED, -1);
		SendMessage(g_hwnd, WM_COMMAND, IDC_EDIT_UNDO, 0);
		ScheduleAction(timeGetTime() + 150, &CSurf_Faderport::ClearUndoLed);
		ClearShift();

		return true;
	}

	bool OnUser(MIDI_event_t * evt) {
		ClearShift();
		return true;
	}


	bool OnShift(MIDI_event_t * evt) {
		m_shift_state = !m_shift_state;
		m_mackie_arrow_states ^= 64;
		m_midiout->Send(BTN, B_SHIFTL, m_shift_state ? STATE_PRESSED : STATE_RELEASED, -1);
		m_midiout->Send(BTN, B_SHIFTR, m_shift_state ? STATE_PRESSED : STATE_RELEASED, -1);

		return true;
	}

	bool OnPan(MIDI_event_t * evt) {
		m_flipmode = !m_flipmode;
		m_midiout->Send(BTN, B_PAN, m_flipmode ? 1 : 0, -1);
		CSurf_ResetAllCachedVolPanStates();
		TrackList_UpdateAllExternalSurfaces();

		return true;
	}

	// used to switch mcp/tcp mode i think
	bool OnAll(MIDI_event_t * evt) {
		g_csurf_mcpmode = !g_csurf_mcpmode;
		m_midiout->Send(BTN, B_ALL, !g_csurf_mcpmode ? STATE_PRESSED : STATE_RELEASED, -1);
		TrackList_UpdateAllExternalSurfaces();

		return true;
	}

	// Not implemented yet

	bool OnSMPTEBeats(MIDI_event_t * evt) {
		int* tmodeptr = (int*)projectconfig_var_addr(NULL, __g_projectconfig_timemode2);
		if (tmodeptr && *tmodeptr >= 0)
		{
			(*tmodeptr)++;
			if ((*tmodeptr) > 5)
				(*tmodeptr) = 0;
		}
		else
		{
			tmodeptr = (int*)projectconfig_var_addr(NULL, __g_projectconfig_timemode);

			if (tmodeptr)
			{
				(*tmodeptr)++;
				if ((*tmodeptr) > 5)
					(*tmodeptr) = 0;
			}
		}

		UpdateTimeline();
		Main_UpdateLoopInfo(0);

		return true;
	}

	bool OnRotaryEncoderPush(MIDI_event_t * evt) {
		int trackid = evt->midi_message[1] - 0x20;
		m_pan_lasttouch[trackid] = timeGetTime();

		trackid += 1 + m_allmcus_bank_offset + m_offset;

		MediaTrack* tr = CSurf_TrackFromID(trackid, g_csurf_mcpmode);
		if (tr)
		{
			if (m_flipmode)
			{
				CSurf_SetSurfaceVolume(tr, CSurf_OnVolumeChange(tr, 1.0, false), NULL);
			}
			else
			{
				CSurf_SetSurfacePan(tr, CSurf_OnPanChange(tr, 0.0, false), NULL);
			}
		}
		return true;
	}

	bool OnRecArm(MIDI_event_t* evt) {
		int tid = evt->midi_message[1];
	//	tid += 1 + m_allmcus_bank_offset + m_offset;
		MediaTrack* tr = CSurf_TrackFromID(GetTrackIdFromSelectButton(tid), g_csurf_mcpmode);
		if (tr)
			CSurf_OnRecArmChange(tr, -1);
		return true;
	}

	int GetTrackIdFromSelectButton(int btn_id) {
		int id =0;

		(id == B_SELECT9) ? id = 9 : id = 1+ (btn_id - (B_SELECT1 + m_allmcus_bank_offset + m_offset));

		return id;

		
	}

	bool OnArm(MIDI_event_t* evt) {
		m_arm_state = !m_arm_state;
		ClearSelectBtns();
		m_midiout->Send(BTN, B_ARM, m_arm_state ? STATE_PRESSED : STATE_RELEASED, -1);
		return true;
	}

	//bool OnRecArm(MIDI_event_t * evt) {
	//	SelectedTrack* i = m_selected_tracks;

	//	while (i) {
	//		SelectedTrack* next = i->next;
	//		MediaTrack* track = i->track();

	//		if (track) {
	//			int state = CSurf_OnRecArmChange(track, -1);
	//			CSurf_SetSurfaceRecArm(track, state, NULL);
	//			m_midiout->Send(BTN, B_ARM, state ? STATE_PRESSED : STATE_RELEASED, -1);
	//		}

	//		i = next;
	//	}
	//	
	//	return true;
	//}

	bool OnRecArmDC(MIDI_event_t* evt) {
		//CSurf_ClearAllRecArmed();
		m_midiout->Send(BTN, B_ARM, STATE_RELEASED, -1);

		return true;
	}

	bool OnRecArmAll(MIDI_event_t* evt) {

		return true;
	}

	bool OnKeyModifier(MIDI_event_t * evt) {
		int mask = (1 << (evt->midi_message[1] - 0x46));
		evt->midi_message[2] >= 0x40 ? m_mackie_modifiers |= mask : m_mackie_modifiers &= ~mask;

		return true;
	}

	bool OnTouch(MIDI_event_t * evt) {
		int fader = evt->midi_message[1] - 0x68;
		m_fader_touchstate[fader] = evt->midi_message[2] >= 0x7f;
		m_fader_lasttouch[fader] = 0xFFFFFFFF; // never use this again!

		return true;
	}

	bool OnFunctionKey(MIDI_event_t * evt) {
		ClearShift();

		return true;

		if (!(m_cfg_flags & CONFIG_FLAG_MAPF1F8TOMARKERS)) return false;

		int fkey = evt->midi_message[1] - 0x36;
		int command = (IsKeyDown(VK_CONTROL) ? ID_SET_MARKER1 : ID_GOTO_MARKER1) + fkey;
		SendMessage(g_hwnd, WM_COMMAND, command, 0);

		ClearShift();

		return true;
	}

	struct ButtonHandler {
		unsigned int evt_min;
		unsigned int evt_max; // inclusive
		MidiHandlerFunc func;
		MidiHandlerFunc func_dc;
		MidiHandlerFunc func_shift;
	};

	bool OnButtonPress(MIDI_event_t * evt) {
		if ((evt->midi_message[0] & 0xf0) != 0x90)
			return false;

		static const int nHandlers = 41;
		static const int nPressOnlyHandlers = 40;
		static const ButtonHandler handlers[nHandlers] = {
		//	{ B_ARM, B_ARM,			  	 &CSurf_Faderport::OnRecArm,        &CSurf_Faderport::OnRecArmDC },
			{ B_ARM, B_ARM,			  	 &CSurf_Faderport::OnArm,        &CSurf_Faderport::OnRecArmDC },

			{ B_SOLO_CLEAR, B_SOLO_CLEAR,&CSurf_Faderport::OnSoloClear },
			{ B_MUTE_CLEAR, B_MUTE_CLEAR,&CSurf_Faderport::OnMuteClear },

			{ B_SELECT1, B_SELECT8,		 &CSurf_Faderport::OnChannelSelect, &CSurf_Faderport::OnChannelSelectDC },
			{ B_SELECT9, B_SELECT9,		 &CSurf_Faderport::OnChannelSelect, &CSurf_Faderport::OnChannelSelectDC },
			{ B_SELECT10, B_SELECT16,	 &CSurf_Faderport::OnChannelSelect, &CSurf_Faderport::OnChannelSelectDC },

			{ B_MUTE1, B_MUTE8,			 &CSurf_Faderport::OnMute },
			{ B_MUTE9, B_MUTE16,		 &CSurf_Faderport::OnMute },
			{ B_SOLO1, B_SOLO8,			 &CSurf_Faderport::OnSolo,			&CSurf_Faderport::OnSoloDC },
			{ B_SOLO9, B_SOLO11,		 &CSurf_Faderport::OnSolo,			&CSurf_Faderport::OnSoloDC },
			{ B_SOLO12, B_SOLO12,		 &CSurf_Faderport::OnSolo,			&CSurf_Faderport::OnSoloDC },
			{ B_SOLO13, B_SOLO14,		 &CSurf_Faderport::OnSolo,			&CSurf_Faderport::OnSoloDC },
			{ B_SOLO15, B_SOLO15,		 &CSurf_Faderport::OnSolo,			&CSurf_Faderport::OnSoloDC },
			{ B_SOLO16, B_SOLO16,		 &CSurf_Faderport::OnSolo,			&CSurf_Faderport::OnSoloDC },

			// Press down only events
			{ B_AUTO_LATCH, B_SAVE,		 &CSurf_Faderport::OnAutoMode,		NULL, &CSurf_Faderport::OnSave },
			{ B_AUTO_TRIM, B_REDO,		 &CSurf_Faderport::OnAutoMode,		NULL, &CSurf_Faderport::OnRedo },
			{ B_AUTO_OFF, B_AUTO_OFF,	 &CSurf_Faderport::OnAutoMode,		NULL, &CSurf_Faderport::OnUndo },
			{ B_AUTO_TOUCH, B_AUTO_TOUCH,&CSurf_Faderport::OnAutoMode,		NULL, &CSurf_Faderport::OnUser },
			{ B_AUTO_WRITE, B_AUTO_WRITE,&CSurf_Faderport::OnAutoMode,		NULL, &CSurf_Faderport::OnUser },
			{ B_AUTO_READ, B_AUTO_READ,  &CSurf_Faderport::OnAutoMode,		NULL, &CSurf_Faderport::OnUser },

			{ B_ENCODER_PUSH, B_ENCODER_PUSH, &CSurf_Faderport::OnRotaryEncoderPush },
			{ B_PREV, B_NEXT,			 &CSurf_Faderport::OnPrevNext },

			{ B_CHANNEL, B_CHANNEL,		 &CSurf_Faderport::OnChannel,       NULL, &CSurf_Faderport::OnFunctionKey },
			{ B_ZOOM, B_ZOOM,			 &CSurf_Faderport::OnZoom,		    NULL, &CSurf_Faderport::OnFunctionKey },
			{ B_SCROLL, B_SCROLL,		 &CSurf_Faderport::OnScroll,	    NULL, &CSurf_Faderport::OnFunctionKey },
			{ B_BANK, B_BANK,			 &CSurf_Faderport::OnBank,		    NULL, &CSurf_Faderport::OnFunctionKey },
			{ B_MASTER, B_MASTER,		 &CSurf_Faderport::OnMaster,		NULL, &CSurf_Faderport::OnFunctionKey },
			{ B_CLICK, B_CLICK,			 &CSurf_Faderport::OnClick,         NULL, &CSurf_Faderport::OnFunctionKey },
			{ B_SECTION, B_SECTION,	     &CSurf_Faderport::OnSection, NULL, &CSurf_Faderport::OnFunctionKey },
			{ B_MARKER, B_MARKER,		 &CSurf_Faderport::OnMarker, NULL, &CSurf_Faderport::OnFunctionKey },


			{ B_CYCLE, B_CYCLE,			 &CSurf_Faderport::OnTransport },
			{ B_RR, B_RR,				 &CSurf_Faderport::OnTransport },
			{ B_FF, B_FF,				 &CSurf_Faderport::OnTransport },
			{ B_STOP, B_STOP,			 &CSurf_Faderport::OnTransport },
			{ B_PLAY, B_PLAY,			 &CSurf_Faderport::OnTransport },
			{ B_RECORD, B_RECORD,		 &CSurf_Faderport::OnTransport },

			{ B_SHIFTL ,B_SHIFTL,		 &CSurf_Faderport::OnShift },
			{ B_SHIFTR ,B_SHIFTR,		 &CSurf_Faderport::OnShift },

			{ B_PAN, B_PAN,				 &CSurf_Faderport::OnPan },
			{ B_ALL, B_USER,			 &CSurf_Faderport::OnAll },

			// Press and release events
			//{ 0x46, 0x49, &CSurf_Faderport::OnKeyModifier },
			{ 0x68, 0x70, &CSurf_Faderport::OnTouch },
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
				ButtonHandler bh = handlers[i];

				if (bh.evt_min <= evt_code && evt_code <= bh.evt_max) {

					// Test for shift
					if (m_shift_state && bh.func_shift != NULL)
						if ((this->*bh.func_shift)(evt))
							return true;

					// Test double-click
					if (double_click && bh.func_dc != NULL)
						if ((this->*bh.func_dc)(evt))
							return true;

					// Single click (and unhandled double clicks)
					if (bh.func != NULL)
						if ((this->*bh.func)(evt))
							return true;
				}
			}
		}

		// For these events we want press and release
		for (int i = nPressOnlyHandlers; i < nHandlers; i++)
			if (handlers[i].evt_min <= evt_code && evt_code <= handlers[i].evt_max)
				if ((this->*handlers[i].func)(evt)) return true;

		// Pass thru if not otherwise handled
		if (evt->midi_message[2] >= 0x40) {
			int a = evt->midi_message[1];
			MIDI_event_t evt = { 0,3,{0xbf - (m_mackie_modifiers & 15),a,0} };
			kbd_OnMidiEvent(&evt, -1);
		}

		return true;
	}

	void OnMIDIEvent(MIDI_event_t * evt)
	{
		static const int nHandlers = 4;
		static const MidiHandlerFunc handlers[nHandlers] = {
				//&CSurf_Faderport::OnFaderportReset,
				&CSurf_Faderport::OnFaderMove,
				&CSurf_Faderport::OnRotaryEncoder,
				&CSurf_Faderport::OnJogWheel,
				&CSurf_Faderport::OnButtonPress,
		};

		for (int i = 0; i < nHandlers; i++)
			if ((this->*handlers[i])(evt)) return;
	}

#pragma endregion Faderport_Events

#pragma region Faderport_Functions

	void UpdateMetronomLED() {
		if (m_metronom_offset)
		{
			int* metro_en = (int*)projectconfig_var_addr(NULL, m_metronom_offset);

			if (metro_en) {
				int a = *metro_en;

				m_midiout->Send(BTN, B_CLICK, a & 1 ? STATE_PRESSED : STATE_RELEASED, -1);
			}
		}
	}

	void ClearSaveLed() {
		m_midiout->Send(BTN, B_SAVE, 0, -1);
	}

	void ClearUndoLed() {
		m_midiout->Send(BTN, B_UNDO, 0, -1);
	}

	bool SetCycleState() {
		int state = (int)GetSetRepeat(-1) == 1 ? 1:0;
		m_midiout->Send(BTN, B_CYCLE, state ? STATE_PRESSED : STATE_RELEASED, -1);

		return state;
	}

	bool AnySolo() {
		for (TrackIterator ti; !ti.end(); ++ti) {
			MediaTrack* track = *ti;
			int* OriginalState = (int*)GetSetMediaTrackInfo(track, "I_SOLO", NULL);

			if (*OriginalState > 0)
				return true;
		}

		return false;
	}

	bool AnyMute() {
		for (TrackIterator ti; !ti.end(); ++ti) {
			MediaTrack* track = *ti;
			int* OriginalState = (int*)GetSetMediaTrackInfo(track, "B_MUTE", NULL);

			if (*OriginalState > 0)
				return true;
		}

		return false;
	}

	void SetBtnColour(int btn_id, int rgb_colour) {

		if (m_midiout == NULL) return;
		
		unsigned int red = (rgb_colour >> 0) & 0xff;
		unsigned int green = (rgb_colour >> 8) & 0xff;
		unsigned int blue = (rgb_colour >> 16) & 0xff;

		m_midiout->Send(RED_CHANNEL, btn_id, 0x00+(red > 0x7F ? (int)red*0.5 : red), -1);
		m_midiout->Send(GREEN_CHANNEL, btn_id, 0x00 + (green > 0x7F ? (int)green * 0.5 : green), -1);
		m_midiout->Send(BLUE_CHANNEL, btn_id, 0x00 + (blue > 0x7F ? (int)blue * 0.5 : blue), -1);

	}


#pragma endregion Faderport_Functions

#pragma region Reaper_Events
public:

	CSurf_Faderport(bool isfp8, int offset, int size, int indev, int outdev, int cfgflags, int* errStats)
	{
		m_cfg_flags = cfgflags;

		m_mcu_list.Add(this);
		m_offset = offset;
		m_midi_in_dev = indev;
		m_midi_out_dev = outdev;

		m_is_fp8 = isfp8;

		m_is_fp8 ? m_size = 8 : m_size = 16;

		// init locals
		int x;
		for (x = 0; x < sizeof(m_mcu_meterpos) / sizeof(m_mcu_meterpos[0]); x++)
			m_mcu_meterpos[x] = -100000.0;
		
		m_mcu_timedisp_lastforce = 0;
		m_mcu_meter_lastrun = 0;
		memset(m_fader_touchstate, 0, sizeof(m_fader_touchstate));
		memset(m_fader_lasttouch, 0, sizeof(m_fader_lasttouch));
		memset(m_pan_lasttouch, 0, sizeof(m_pan_lasttouch));

		//create midi hardware access
		m_midiin = m_midi_in_dev >= 0 ? CreateMIDIInput(m_midi_in_dev) : NULL;
		m_midiout = m_midi_out_dev >= 0 ? CreateThreadedMIDIOutput(CreateMIDIOutput(m_midi_out_dev, false, NULL)) : NULL;

		if (errStats)
		{
			if (m_midi_in_dev >= 0 && !m_midiin) *errStats |= 1;
			if (m_midi_out_dev >= 0 && !m_midiout) *errStats |= 2;
		}

		FaderportReset();

		if (m_midiin)
			m_midiin->start();

		m_repos_faders = false;
		m_schedule = NULL;
		m_selected_tracks = NULL;
	}

	// Destructor
	~CSurf_Faderport()
	{
		m_mcu_list.Delete(m_mcu_list.Find(this));
		if (m_midiout == NULL) return;

		//ResetAll();

		//for(int id=0; id<m_size; id++)
		//	ResetScribble(id);

		delete m_midiout;
		delete m_midiin;

		while (m_schedule != NULL) {
			ScheduledAction* temp = m_schedule;
			m_schedule = temp->next;
			delete temp;
		}

		while (m_selected_tracks != NULL) {
			SelectedTrack* temp = m_selected_tracks;
			m_selected_tracks = temp->next;
			delete temp;
		}
	}

	const char* GetTypeString() { return m_is_fp8 ? "FADERPORT8" : "FADERPORT16"; }
	const char* GetDescString()
	{
		m_descspace.Set("Presonus Faderport 8/16");
		char tmp[512];
		sprintf(tmp, " (dev %d,%d)", m_midi_in_dev, m_midi_out_dev);
		m_descspace.Append(tmp);
		return m_descspace.Get();
	}

	const char* GetConfigString() // string of configuration data
	{
		sprintf(m_configtmp, "%d %d %d %d %d", m_offset, m_size, m_midi_in_dev, m_midi_out_dev, m_cfg_flags);
		return m_configtmp;
	}

	//unused
	void CloseNoReset()
	{
		delete m_midiout;
		delete m_midiin;
		m_midiout = 0;
		m_midiin = 0;
	}

	void Run()
	{

		if (m_midiout == NULL) return;

		DWORD now = timeGetTime();

		if (now >= m_frameupd_lastrun + (1000 / max((*g_config_csurf_rate), 1)) || now < m_frameupd_lastrun - 250)
		{
			m_frameupd_lastrun = now;

			while (m_schedule && now >= m_schedule->time) {
				ScheduledAction* action = m_schedule;
				m_schedule = m_schedule->next;
				(this->*(action->func))();
				delete action;
			}

			double pp = (GetPlayState() & 1) ? GetPlayPosition() : GetCursorPosition();
			unsigned char bla[10];

			memset(bla, 0, sizeof(bla));

			int* tmodeptr = (int*)projectconfig_var_addr(NULL, __g_projectconfig_timemode2);

			int tmode = 0;

			if (tmodeptr && (*tmodeptr) >= 0) tmode = *tmodeptr;
			else
			{
				tmodeptr = (int*)projectconfig_var_addr(NULL, __g_projectconfig_timemode);
				if (tmodeptr)
					tmode = *tmodeptr;
			}

			if (tmode == 3) // seconds
			{
				double* toptr = (double*)projectconfig_var_addr(NULL, __g_projectconfig_timeoffs);

				if (toptr) pp += *toptr;
				char buf[64];
				sprintf(buf, "%d %02d", (int)pp, ((int)(pp * 100.0)) % 100);
				if (strlen(buf) > sizeof(bla)) memcpy(bla, buf + strlen(buf) - sizeof(bla), sizeof(bla));
				else
					memcpy(bla + sizeof(bla) - strlen(buf), buf, strlen(buf));
			}
			else if (tmode == 4) // samples
			{
				char buf[128];
				format_timestr_pos(pp, buf, sizeof(buf), 4);
				if (strlen(buf) > sizeof(bla)) memcpy(bla, buf + strlen(buf) - sizeof(bla), sizeof(bla));
				else
					memcpy(bla + sizeof(bla) - strlen(buf), buf, strlen(buf));
			}
			else if (tmode == 5) // frames
			{
				char buf[128];
				format_timestr_pos(pp, buf, sizeof(buf), 5);
				char* p = buf;
				char* op = buf;
				int ccnt = 0;
					
				while (*p)
				{
					if (*p == ':')
					{
						ccnt++;

						if (ccnt != 3)
						{
							p++;
							continue;
						}

						*p = ' ';
					}

					*op++ = *p++;
				}

				*op = 0;

				strlen(buf) > sizeof(bla) ? memcpy(bla, buf + strlen(buf) - sizeof(bla), sizeof(bla)) : memcpy(bla + sizeof(bla) - strlen(buf), buf, strlen(buf));
			}
			else if (tmode > 0)
			{
				int num_measures = 0;
				double beats = TimeMap2_timeToBeats(NULL, pp, &num_measures, NULL, NULL, NULL) + 0.000000000001;
				double nbeats = floor(beats);

				beats -= nbeats;

				int fracbeats = (int)(1000.0 * beats);

				int* measptr = (int*)projectconfig_var_addr(NULL, __g_projectconfig_measoffs);
				int nm = num_measures + 1 + (measptr ? *measptr : 0);
				if (nm >= 100) bla[0] = '0' + (nm / 100) % 10;//bars hund
				if (nm >= 10) bla[1] = '0' + (nm / 10) % 10;//barstens
				bla[2] = '0' + (nm) % 10;//bars

				int nb = (int)nbeats + 1;
				if (nb >= 10) bla[3] = '0' + (nb / 10) % 10;//beats tens
				bla[4] = '0' + (nb) % 10;//beats

				bla[7] = '0' + (fracbeats / 100) % 10;
				bla[8] = '0' + (fracbeats / 10) % 10;
				bla[9] = '0' + (fracbeats % 10); // frames
			}
			else
			{
				double* toptr = (double*)projectconfig_var_addr(NULL, __g_projectconfig_timeoffs);
				if (toptr) pp += (*toptr);

				int ipp = (int)pp;
				int fr = (int)((pp - ipp) * 1000.0);

				if (ipp >= 360000) bla[0] = '0' + (ipp / 360000) % 10;//hours hundreds
				if (ipp >= 36000) bla[1] = '0' + (ipp / 36000) % 10;//hours tens
				if (ipp >= 3600) bla[2] = '0' + (ipp / 3600) % 10;//hours

				bla[3] = '0' + (ipp / 600) % 6;//min tens
				bla[4] = '0' + (ipp / 60) % 10;//min
				bla[5] = '0' + (ipp / 10) % 6;//sec tens
				bla[6] = '0' + (ipp % 10);//sec
				bla[7] = '0' + (fr / 100) % 10;
				bla[8] = '0' + (fr / 10) % 10;
				bla[9] = '0' + (fr % 10); // frames
			}

			if (m_mackie_lasttime_mode != tmode)
			{
				m_mackie_lasttime_mode = tmode;
				//m_midiout->Send(BTN, 0x71, tmode == 5 ? 0x7F : 0, -1); // set smpte light
				//m_midiout->Send(BTN, 0x72, m_mackie_lasttime_mode > 0 && tmode < 3 ? 0x7F : 0, -1); // set beats light
			}

			bool force = false;
			if (now > m_mcu_timedisp_lastforce)
			{
				m_mcu_timedisp_lastforce = now + 2000;
				force = true;
			}

			for (int x = 0; x < sizeof(bla); x++)
			{
				int track_id = sizeof(bla) - x - 1;
				if (bla[track_id] != m_mackie_lasttime[track_id] || force)
				{
					m_mackie_lasttime[track_id] = bla[track_id];
				}
			}

			PlayState(now);
		}

		UpdateArrowStates(now);

		for (int strip_id = 0; strip_id < m_size; strip_id++) {
			SetBtnSelectColourFromTrackColour(strip_id);
		}
	
	}

	void SetBtnSelectColourFromTrackColour(int strip_id) {
		if (m_arm_state) {
			SetArmedBtnColor();
			return;
		}

		MediaTrack* t;

		if ((t = CSurf_TrackFromID(strip_id + m_offset + m_allmcus_bank_offset + 1, g_csurf_mcpmode)) == NULL) return;
		{
			unsigned int* rgb = (unsigned int*)GetSetMediaTrackInfo(t, "I_CUSTOMCOLOR", NULL);
			SetBtnColour(TrackIdToSelectBtn(strip_id), *rgb);
		}
	}
	
	void PlayState(DWORD now) {

		if (!(GetPlayState() & 1)) return;

#define VU_BOTTOM 70
		double decay = 0.0;

		if (m_mcu_meter_lastrun)
		{
			decay = VU_BOTTOM * (double)(now - m_mcu_meter_lastrun) / (1.4 * 1000.0);   // they claim 1.8s for falloff but we'll underestimate
		}

		m_mcu_meter_lastrun = now;

		for (int strip_id = 0; strip_id < m_size; strip_id++)
		{
			int track_id = m_offset + m_allmcus_bank_offset + strip_id + 1;
			MediaTrack* t;

			if ((t = CSurf_TrackFromID(track_id, g_csurf_mcpmode)))
			{
				double pp = VAL2DB((Track_GetPeakInfo(t, 0) + Track_GetPeakInfo(t, 1)) * 0.5);

				if (m_mcu_meterpos[strip_id] > -VU_BOTTOM * 2) m_mcu_meterpos[strip_id] -= decay;

				if (pp < m_mcu_meterpos[strip_id]) continue;

				m_mcu_meterpos[strip_id] = pp;

				//int v = 0xd; // 0xe turns on clip indicator, 0xf turns it off
				int v = 0x0;

				if (pp < 0.0)
					pp < -VU_BOTTOM ? v=0x0 : v = (int)((pp + VU_BOTTOM) * 13.0 / VU_BOTTOM) * 10;

				UpdateMeter(strip_id,v);
			}
		}
	}

	void UpdateMeter(int strip_id, int v) {
		if (m_midiout == NULL) return;

		if (strip_id < 8) m_midiout->Send(PEAK_METER_1_8 + strip_id, v, 0, -1);

		if (!m_is_fp8 && strip_id > 7) m_midiout->Send(PEAK_METER_9_16 + (strip_id & 7), v, 0, -1);
	}

	void UpdateArrowStates(DWORD now){

		if (m_midiin == NULL) return;

		m_midiin->SwapBufs(timeGetTime());

		int l = 0;
		
		MIDI_eventlist* list = m_midiin->GetReadBuf();
		MIDI_event_t* evts;
		
		while ((evts = list->EnumItems(&l))) OnMIDIEvent(evts);

		if (m_mackie_arrow_states)
		{
			DWORD now = timeGetTime();
			if (now >= m_buttonstate_lastrun + 100)
			{
				m_buttonstate_lastrun = now;

				if (m_mackie_arrow_states)
				{
					int iszoom = m_mackie_arrow_states & 64;

					if (m_mackie_arrow_states & 1)
						CSurf_OnArrow(0, !!iszoom);
					if (m_mackie_arrow_states & 2)
						CSurf_OnArrow(1, !!iszoom);
					if (m_mackie_arrow_states & 4)
						CSurf_OnArrow(2, !!iszoom);
					if (m_mackie_arrow_states & 8)
						CSurf_OnArrow(3, !!iszoom);
				}
			}
		}

		if (m_repos_faders && now >= m_fader_lastmove + FADER_REPOS_WAIT) {
			m_repos_faders = false;
			TrackList_UpdateAllExternalSurfaces();
		}
	}

	void SetTrackListChange()
	{
		if (m_midiout == NULL) return;

		for (int strip_id = 0; strip_id < m_size; strip_id++)
		{
			MediaTrack* track = CSurf_TrackFromID(strip_id + m_offset + m_allmcus_bank_offset + 1, g_csurf_mcpmode);

			if (track != NULL) {
				ResetValueBar(strip_id, MODE_NORMAL);
			}

			if (!track || track == CSurf_TrackFromID(0, false))
			{
				m_vol_lastpos[strip_id] = m_flipmode ? panToInt14(0.0) : volToInt14(0.0);
					
				ResetFader(strip_id);
				ClearScribbleText(strip_id);
				ResetValueBar(strip_id, MODE_OFF);
				ResetScribble(strip_id);
				ClearSoloBtn(strip_id);
				ClearMuteBtn(strip_id);
				ClearMeter(strip_id);
				ClearSelect(strip_id);
			}
		}
	}

#define FIXID(id) int id=CSurf_TrackToID(trackid,g_csurf_mcpmode);  \
				  int oid=id;  \
                  if (id>0) { \
					id -= m_allmcus_bank_offset+1; \
                    if (id==m_size) id=-1; \
				  } else if (id==0) id=m_size;

	void SetSurfaceVolume(MediaTrack* trackid, double volume)
	{
		FIXID(id)
			if (m_midiout == NULL || id < 0 || id >= 256 || id >= m_size) return;

			//if (m_flipmode)
			//{
			//	unsigned char volch = volToChar(volume);
			//	//if (id < NUM_FADERS)
			//	//	m_midiout->Send(VALUE_BAR, 0x30 + (id & 0xf), 1 + ((volch * 11) >> 7), -1);
			//}
			//else
			{
				int volint = volToInt14(volume);

				if (m_vol_lastpos[id] != volint)
				{
					m_vol_lastpos[id] = volint;
					m_midiout->Send(FADER + (id), volint & 0x7F, (volint >> 7) & 0x7F, -1); // send fader value to surface
				}
			}
	}

	void SetSurfacePan(MediaTrack* trackid, double pan)
	{
		FIXID(id)
			//if (m_midiout && id >= 0 && id < 256 && id < m_size)
			if (m_midiout == NULL || id < 0 || id >= 256 || id >= m_size) return;
			
			unsigned char panch = panToChar(pan);

			if (m_pan_lastpos[id] == panch) return;
			
			m_pan_lastpos[id] = panch;

			if (m_flipmode) // SET PAN USING FADERS
			{
				int panint = panToInt14(pan);

				if (m_vol_lastpos[id] != panint)
				{
					m_vol_lastpos[id] = panint;
					m_midiout->Send(FADER + (id), panint & 0x7F, (panint >> 7) & 0x7F, -1);
				}

				return;
			}

			if (id >= m_size) return;

			int panint = 127-panToInt14(pan); // change direction

			if(id < 8) m_midiout->Send(VALUEBAR, 0x30 + (id), (panint >> 7) & 0x7F, 0);  // scribble 1-8

			if(id > 7 && !m_is_fp8) m_midiout->Send(VALUEBAR, 0x40 + (id-8), (panint >> 7) & 0x7F, 0); //scribble 9-16
	}

	void SetSurfaceMute(MediaTrack* trackid, bool mute)
	{
		FIXID(id)

			if (m_midiout == NULL || id < 0 || id >= 256 || id >= m_size) return;

			int muteBtnOffset = B_MUTE1;

			if (id > 8) muteBtnOffset = B_MUTE9-0x08;

			m_midiout->Send(BTN, muteBtnOffset + id, mute ? STATE_PRESSED : STATE_RELEASED, -1);

			AnyMute() ? m_midiout->Send(BTN, B_MUTE_CLEAR, STATE_PRESSED, -1) : m_midiout->Send(BTN, B_MUTE_CLEAR, STATE_RELEASED, -1);
	}

	void SetSurfaceSolo(MediaTrack* trackid, bool solo)
	{
		FIXID(id)

			if (m_midiout == NULL || id < 0 || id >= 256 || id >= m_size) return;

			int send_code = B_SOLO1 + (id);

			// rules for buttons 9-16
			if (id > 7 && id < 14) send_code = 0x48 + (id);

			if (id == 11) send_code = B_SOLO12; // solo 12

			if (id == 14) send_code = B_SOLO15; // solo 15

			if (id == 15) send_code = B_SOLO16; // solo 16

			m_midiout->Send(BTN, send_code, solo ? STATE_BLINK : 0, -1); //blink

			// Set Solo clear depending on if any tracks are soloed or not
			AnySolo() ? m_midiout->Send(BTN, B_SOLO_CLEAR, STATE_PRESSED, -1) : m_midiout->Send(BTN, B_SOLO_CLEAR, STATE_RELEASED, -1); 
	}

	// Update the surface select LEDs
	void SetSurfaceSelected(MediaTrack* trackid, bool selected)
	{
		selected ? selectTrack(trackid) : deselectTrack(trackid);

		FIXID(id)

			if (m_midiout == NULL || id < 0 || id >= 256 || id >= m_size) return;

			m_midiout->Send(BTN, TrackIdToSelectBtn(id), selected ? STATE_PRESSED : STATE_RELEASED, -1);

			UpdateAutoModes();
	}



	int TrackIdToSelectBtn(int id) {
		return	id == 8 ? id = B_SELECT9 : id += B_SELECT1;
	}

	void selectTrack(MediaTrack* track_id) {
		const GUID* guid = GetTrackGUID(track_id);

		// Empty list, start new list
		if (m_selected_tracks == NULL) {
			m_selected_tracks = new SelectedTrack(track_id);
			return;
		}

		// This track is head of list
		if (guid && !memcmp(&m_selected_tracks->guid, guid, sizeof(GUID)))
			return;

		// Scan for track already selected
		SelectedTrack* i = m_selected_tracks;

		while (i->next) {
			i = i->next;

			if (guid && !memcmp(&i->guid, guid, sizeof(GUID)))
				return;
		}

		// Append at end of list if not already selected
		i->next = new SelectedTrack(track_id);
	}

	void deselectTrack(MediaTrack* trackid) {
		const GUID* guid = GetTrackGUID(trackid);

		if (!m_selected_tracks) return;

		// This track is head of list?
		if (guid && !memcmp(&m_selected_tracks->guid, guid, sizeof(GUID))) {
			SelectedTrack* tmp = m_selected_tracks;
			m_selected_tracks = m_selected_tracks->next;
			delete tmp;
			return;
		}

		SelectedTrack* i = m_selected_tracks;

		while (i->next) {
			if (guid && !memcmp(&i->next->guid, guid, sizeof(GUID))) {
				SelectedTrack* tmp = i->next;
				i->next = i->next->next;
				delete tmp;
				break;
			}

			i = i->next;
		}
	}

	void SetSurfaceRecArm(MediaTrack* trackid, bool recarm)
	{
		FIXID(id)
			if (m_midiout && id >= 0 && id < 256 && id < m_size)
			{
				if (id < 16 && m_arm_state)
				{
					m_midiout->Send(BTN, TrackIdToSelectBtn(id), recarm ? 0x7f : 0, -1);
				}
			}
	}

	void SetPlayState(bool play, bool pause, bool rec)
	{
		if (m_midiout == NULL) return;

		m_midiout->Send(BTN, B_RECORD, rec ? STATE_PRESSED : 0, -1);
		m_midiout->Send(BTN, B_PLAY, play || pause ? STATE_PRESSED : 0, -1);
		m_midiout->Send(BTN, B_STOP, !play ? STATE_PRESSED : 0, -1);
	}
	
	void SetTrackTitle(MediaTrack* trackid, const char* title)
	{
		FIXID(id)
			if (m_midiout == NULL) return;

			char buf[32];
			memcpy(buf, title, 6);
			buf[6] = 0;

			// Does this ever happen?
			//if (strlen(buf) == 0) {
			//	int track_id = CSurf_TrackToID(trackid, g_csurf_mcpmode);

			//	track_id < 100 ? sprintf(buf, "  %02d  ", track_id) : sprintf(buf, "  %d ", track_id);
			//}

			SendTextToScribble(id, buf);
	}

	bool GetTouchState(MediaTrack* trackid, int isPan = 0)
	{
		FIXID(id)
			if (!m_flipmode != !isPan)
			{
				if (id >= 0 && id < 8)
				{
					DWORD now = timeGetTime();
					if (m_pan_lasttouch[id] == 1 || (now < m_pan_lasttouch[id] + 3000 && now >= m_pan_lasttouch[id] - 1000)) // fake touch, go for 3s after last movement
					{
						return true;
					}
				}
				return false;
			}

			if (id >= 0 && id < 9)
			{
				if (!(m_cfg_flags & CONFIG_FLAG_FADER_TOUCH_MODE) && !m_fader_touchstate[id] && m_fader_lasttouch[id] && m_fader_lasttouch[id] != 0xffffffff)
				{
					DWORD now = timeGetTime();
					if (now < m_fader_lasttouch[id] + 3000 && now >= m_fader_lasttouch[id] - 1000) return true;
					return false;
				}

				return !!m_fader_touchstate[id];
			}

		return false;
	}

	void SetAutoMode(int mode)
	{
		UpdateAutoModes();
	}

	void UpdateAutoModes() {
		if (m_midiout == NULL) return;
			
		int modes[5] = { 0, 0, 0, 0, 0 };
			
		for (SelectedTrack* i = m_selected_tracks; i; i = i->next) {
			
			MediaTrack* track = i->track();
				
			if (!track) continue;
				
			int mode = GetTrackAutomationMode(track);
				
			if (0 <= mode && mode < 5)
				modes[mode] = 1;
		}

		bool multi = (modes[0] + modes[1] + modes[2] + modes[3] + modes[4]) > 1;
			
		m_midiout->Send(BTN, B_AUTO_READ, modes[AUTO_MODE_READ] ? (multi ? 1 : STATE_PRESSED) : 0, -1);
		m_midiout->Send(BTN, B_AUTO_WRITE, modes[AUTO_MODE_WRITE] ? (multi ? 1 : STATE_PRESSED) : 0, -1);
		m_midiout->Send(BTN, B_AUTO_TRIM, modes[AUTO_MODE_TRIM] ? (multi ? 1 : STATE_PRESSED) : 0, -1);
		m_midiout->Send(BTN, B_AUTO_TOUCH, modes[AUTO_MODE_TOUCH] ? (multi ? 1 : STATE_PRESSED) : 0, -1);
		m_midiout->Send(BTN, B_AUTO_LATCH, modes[AUTO_MODE_LATCH] ? (multi ? 1 : STATE_PRESSED) : 0, -1);
	}

	void ResetCachedVolPanStates()
	{
		memset(m_vol_lastpos, 0xff, sizeof(m_vol_lastpos));
		memset(m_pan_lastpos, 0xff, sizeof(m_pan_lastpos));
	}

	void OnTrackSelection(MediaTrack* trackid)
	{
		int tid = CSurf_TrackToID(trackid, g_csurf_mcpmode);
		// if no normal MCU's here, then slave it
		int movesize = m_size;
		
		// seems that it finds all the connected controllers and banks accordingly
		// need to decide how to handle this
		// focus on a single controller for now

		//for (x = 0; x < m_mcu_list.GetSize(); x++)
		//{
		//	CSurf_Faderport* mcu = m_mcu_list.Get(x);
		//	if (mcu)
		//	{
		//		if (mcu->m_offset + 8 > movesize)
		//			movesize = mcu->m_offset + 8;
		//	}
		//}

		int newpos = tid - 1;
		if (newpos >= 0 && (newpos < m_allmcus_bank_offset || newpos >= m_allmcus_bank_offset + movesize))
		{
			int no = newpos - (newpos % movesize);

			if (no != m_allmcus_bank_offset)
			{
				m_allmcus_bank_offset = no;

				// update all of the sliders
				TrackList_UpdateAllExternalSurfaces();

				for (int list_id = 0; list_id < m_mcu_list.GetSize(); list_id++)
				{
					CSurf_Faderport* mcu = m_mcu_list.Get(list_id);
					if (mcu && mcu->m_midiout)
					{
				
						// send the pan and volume information to each surface????

						//mcu->m_midiout->Send(VALUE_BAR, 0x40 + 11, '0' + (((m_allmcus_bank_offset + 1) / 10) % 10), -1);
						//mcu->m_midiout->Send(VALUE_BAR, 0x40 + 10, '0' + ((m_allmcus_bank_offset + 1) % 10), -1);

						//m_midiout->Send(VALUE_BAR, 0x37 + x, 4, -1); // value bar tracks 1-8 (offset by 8) turn off =4
						//m_midiout->Send(VALUE_BAR, 0x40 + x, 4, -1); // value bar tracks 9-16 (offset by 8)  turn off = 4
					}
				}
			}
		}
	}

	bool IsKeyDown(int key)
	{
		if (!m_midiin) return false;

		if (key == VK_SHIFT) return !!(m_mackie_modifiers & 1);
		if (key == VK_CONTROL) return !!(m_mackie_modifiers & 4);
		if (key == VK_MENU) return !!(m_mackie_modifiers & 8);

	}

};
#pragma endregion Reaper_Events

static IReaperControlSurface* createFunc(const char* type_string, const char* configString, int* errStats)
{
	int parms[5];
	parseParms(configString, parms);

	return new CSurf_Faderport(!strcmp(type_string, "FADERPORT8"), parms[0], parms[1], parms[2], parms[3], parms[4], errStats);
}


static HWND configFunc(const char* type_string, HWND parent, const char* initConfigString)
{
	return CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_SURFACEEDIT_MCU1), parent, dlgProc, (LPARAM)initConfigString);
}

reaper_csurf_reg_t csurf_faderport16_reg =
{
  "FADERPORT16",
  "Presonus Faderport 16",
  createFunc,
  configFunc,
};

reaper_csurf_reg_t csurf_faderport8_reg =
{
  "FADERPORT8",
  "Presonus Faderport 8",
  createFunc,
  configFunc,
};
