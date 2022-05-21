/*
** reaper_csurf
** My Device support
** Copyright (C) 2006-2018
** License: LGPL.
*/
#include "csurf_faderport.h"
#include "config_file.h"
#include <fstream>
#include <iostream>
#include <string>
using namespace std;

// Global state variables allow multiple surfaces to share states
static int g_fader_pan = 0;
static int g_send_state = 0;
static int g_number_of_tracks_last = 0;

static VirtualSurface g_vs_busview;
static VirtualSurface g_vs_allview;
static VirtualSurface g_vs_vcaview;
static VirtualSurface g_vs_audioview;
static VirtualSurface g_vs_viview;
static VirtualSurface* g_vs_currentview;

#pragma region midi
void CSurf_Faderport::OnMidiEvent(MIDI_event_t* evt) {
	int evt_code = evt->midi_message[1];

	// Surface events - communicate with Reaper when something happens on the surface e.g a button push lights a button
	// In many cases a button is lit on the surface following a Reaper event and the lightning of the button is not done here
	// but is controlled in the Run() method
	if (OnFaderMoveEvt(evt)) return; // fader move

	if (OnTransportEvt(evt)) return; // transport controls

	if (OnParamFXEncoder(evt)) return; // small encoder

	if (OnSessionNavigationEncoder(evt)) return; // jog wheel

	if (OnAutoMode(evt)) return; // automation buttons

	if (OnTouch(evt)) return; // touch fader

	// Calculate if double-click
	DWORD now = timeGetTime();
	bool isDoubleClick = evt_code == m_button_last && now - m_button_last_time < DOUBLE_CLICK_INTERVAL;
	m_button_last = evt_code;
	m_button_last_time = now;

	if (isAudioEvt(evt)) {
		SetAudioViewState();
		return;
	}

	if (isBusEvt(evt)) {
		SetBusViewState();
		return;
	}

	if (isVIEvt(evt)) {
		SetVIViewState();
		return;
	}

	if (isVCAEvt(evt)) {
		SetVCAViewState();
		return;
	}

	if (isAllEvt(evt)) {
		SetAllViewState();
		return;
	}

	if (isSendsEvt(evt)) {
		SetSendsState();
		return;
	}

	// Button pushes light buttons when pressed
	if (isPanEvt(evt)) {
		SetPanState();
		return;
	}

	if (isMuteEvt(evt)) {
		MuteSelectedTrack(evt_code, isDoubleClick);
		return;
	}

	if (isSoloEvt(evt)) {
		SoloSelectedTrack(evt_code, isDoubleClick);
		return;
	}

	if (isSoloClearEvt(evt)) {
		SoloClearAll(evt_code);
		return;
	}

	if (isMuteClearEvt(evt)) {
		MuteClearAll(evt_code);
		return;
	}

	if (isPrevNextEvt(evt)) {
		PrevNext(evt_code & 1);
		return;
	}

	if (isBankEvt(evt)) {
		SetBankState();
		return;
	}

	if (isChannelEvt(evt)) {
		SetChannelState();
		return;
	}

	if (isChannelSelectEvt(evt)) {
		isDoubleClick ? SelectTrackDoubleClick(evt_code) : SelectTrack(evt_code);
		return;
	}

	//if (isAllEvt(evt)) {
	//	SetMCPTCPView();
	//	return;
	//}

	if (isArmEvt(evt)) {
		isDoubleClick ? ClearArmState() : SetArmState();
		return;
	}

	if (isMasterEvt(evt)) {
		SetMasterState();
		return;
	}

	if (isShiftEvt(evt)) {
		SetShiftState();
		return;
	}

	if (isSave(evt)) {
		Save();
		return;
	}

	if (isRedo(evt)) {
		Redo();
		return;
	}

	if (isUndo(evt)) {
		Undo();
		return;
	}

	if (isLink(evt)) {
		SetLinkLED();
		return;
	}

	if (isMacro(evt)) {
		return;
	}

	// Pass thru if no event found
	if (evt->midi_message[2] >= 0x40) {
		int a = evt->midi_message[1];
		kbd_OnMidiEvent(evt, -1);
	}
}

bool CSurf_Faderport::OnTouch(MIDI_event_t* evt) {
	if (!m_Functions.isTouchFader(evt)) return false;

	int fader = evt->midi_message[1] - 0x68;
	m_fader_touchstate[fader] = evt->midi_message[2] >= 0x7f;
	m_fader_lasttouch[fader] = 0xFFFFFFFF; // never use this again!

	return true;
}

//Surface Events
bool CSurf_Faderport::isPrevNextEvt(MIDI_event_t* evt) {
	int midi_code = evt->midi_message[1];
	return m_Functions.isBtn(evt) && (midi_code == B_PREV || midi_code == B_NEXT);
}

bool CSurf_Faderport::isChannelEvt(MIDI_event_t* evt) {
	int midi_code = evt->midi_message[1];
	return (m_Functions.isBtn(evt) && midi_code == B_CHANNEL);
}

bool CSurf_Faderport::isBankEvt(MIDI_event_t* evt) {
	int midi_code = evt->midi_message[1];
	return (m_Functions.isBtn(evt) && midi_code == B_BANK);
}

bool CSurf_Faderport::isMuteEvt(MIDI_event_t* evt) {
	int midi_code = evt->midi_message[1];
	return (m_Functions.isBtn(evt) && ((midi_code >= B_MUTE1 && midi_code <= B_MUTE8) || (midi_code >= B_MUTE9 && midi_code <= B_MUTE16)));
}

bool CSurf_Faderport::isSoloEvt(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];

	if (midi_code >= B_SOLO1 && midi_code <= B_SOLO8) return true;

	if (midi_code >= B_SOLO9 && midi_code <= B_SOLO11) return true;

	if (midi_code >= B_SOLO16 && midi_code <= B_SOLO15) return true;

	if (midi_code >= B_SOLO13 && midi_code <= B_SOLO14) return true;

	return false;
}

bool CSurf_Faderport::isSoloClearEvt(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];
	return midi_code == B_SOLO_CLEAR;
}

bool CSurf_Faderport::isMuteClearEvt(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];
	return midi_code == B_MUTE_CLEAR;
}

bool CSurf_Faderport::isChannelSelectEvt(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];

	if (midi_code >= B_SELECT1 && midi_code <= B_SELECT8) return true;
	if (midi_code == B_SELECT9) return true;
	if (midi_code >= B_SELECT10 && midi_code <= B_SELECT16) return true;

	return false;
}

bool CSurf_Faderport::isPanEvt(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];

	return (midi_code == B_PAN);
}

bool CSurf_Faderport::isBusEvt(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];

	return (midi_code == B_BUS);
}

bool CSurf_Faderport::isVIEvt(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];

	return (midi_code == B_VI);
}

bool CSurf_Faderport::isAudioEvt(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];

	return (midi_code == B_AUDIO);
}

bool CSurf_Faderport::isVCAEvt(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];

	return (midi_code == B_VCA);
}


bool CSurf_Faderport::isAllEvt(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];

	return (midi_code == B_ALL);
}

bool CSurf_Faderport::isSendsEvt(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];
	return midi_code == B_SENDS;
}

bool CSurf_Faderport::isArmEvt(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];

	return (midi_code == B_ARM);
}

bool CSurf_Faderport::isMasterEvt(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];

	return (midi_code == B_MASTER);
}

bool CSurf_Faderport::isShiftEvt(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];

	return (midi_code == B_SHIFTL || midi_code == B_SHIFTR);
}

bool CSurf_Faderport::isSave(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];
	return (midi_code == B_SAVE) && m_surfaceState.GetShift();
}

bool CSurf_Faderport::isRedo(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];
	return (midi_code == B_REDO) && m_surfaceState.GetShift();
}

bool CSurf_Faderport::isUndo(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];
	return (midi_code == B_UNDO) && m_surfaceState.GetShift();
}

bool CSurf_Faderport::isUser1(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];
	return (midi_code == B_USER1);
}

bool CSurf_Faderport::isUser2(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];
	return (midi_code == B_USER2) && m_surfaceState.GetShift();
}

bool CSurf_Faderport::isUser3(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];
	return (midi_code == B_USER3) && m_surfaceState.GetShift();
}

bool CSurf_Faderport::isLink(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];
	return (midi_code == B_LINK);
}

bool CSurf_Faderport::isMacro(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	int midi_code = evt->midi_message[1];
	return (midi_code == B_MACRO);
}

bool CSurf_Faderport::OnFaderMoveEvt(MIDI_event_t* evt) {
	if (!m_Functions.isFader(evt)) return false;

	int track_id = (int)(evt->midi_message[0] & 0xf);
	int surface_size = m_surfaceState.GetSurfaceSize();

	if (track_id > 32 || track_id > surface_size) return false;

	m_fader_lastmove = timeGetTime();
	m_fader_lasttouch[track_id] != 0xffffffff ? m_fader_lasttouch[track_id] = m_fader_lastmove : false;

	MediaTrack* track = g_vs_currentview->media_track[track_id];

	if (track == NULL) return false;

	double volume = g_fader_pan ? int14ToPan(evt->midi_message[2], evt->midi_message[1]) : int14ToVol(evt->midi_message[2], evt->midi_message[1]);

	g_fader_pan ? CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, volume, false), NULL) :
		CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, volume, false), NULL);

	return true;
}

// Flaw - you wont be able to pan by banking and selecting doing this
// only pan on the current surface view will be panned

// NOTE - LATEST THIS MUST BE UPDATED TO SUPPORT DIFFERENT METHODS
// ATM WE ONLY CARE ABOUT PAN
bool CSurf_Faderport::OnParamFXEncoder(MIDI_event_t* evt) {
	if (!m_Functions.isParamFXEncoder(evt)) return false;

	//if (!m_Functions.isPan(evt)) return false;

	// We only care about tracks selected on the surface
	// so apply pan to those tracks
	for (int surface_displayid = 0; surface_displayid < m_surfaceState.GetSurfaceSize(); surface_displayid++) {
		MediaTrack* track = g_vs_currentview->media_track[surface_displayid];
		bool surfaceTrackSelected = GetMediaTrackInfo_Value(track, "I_SELECTED");

		if (!surfaceTrackSelected) continue;

		m_pan_lasttouch[surface_displayid] = timeGetTime();
		double adj = (evt->midi_message[2] & 0x3f) / 31.0;

		if (evt->midi_message[2] & 0x40) adj = -adj;

		g_fader_pan ? CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, adj * 11.0, true), NULL) :
			CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, adj, true), NULL);
	}

	return true;
}

// NOTE - LATEST THIS MUST BE UPDATED TO SUPPORT DIFFERENT METHODS
// ATM WE ONLY CARE ABOUT MASTER VOLUME AND SESSION MARKER
bool CSurf_Faderport::OnSessionNavigationEncoder(MIDI_event_t* evt) {
	if (!m_Functions.isSessionNavigationEncoder(evt)) return false;

	// Master button is lit then control volume of track 0
	if (m_surfaceState.GetMaster()) {
		MediaTrack* track = CSurf_TrackFromID(0, m_surfaceState.GetMCPMode());
		double adj = (evt->midi_message[2] & 0x3f) / 31.0;

		if (evt->midi_message[2] & 0x40) adj = -adj;

		CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, adj * 15.0, true), NULL);

		return true;
	}

	// add a modifier for smaller steps
	if (evt->midi_message[2] == 0x41)
		CSurf_OnRew(500);

	if (evt->midi_message[2] == 0x01)
		CSurf_OnFwd(500);

	return true;
}

void CSurf_Faderport::Save() {
	m_Functions.SetBtnColour(B_SAVE, 16802816);
	m_midiout->Send(BTN, B_SAVE, STATE_ON, -1);
	SendMessage(g_hwnd, WM_COMMAND, ID_FILE_SAVEPROJECT, 0);
	//SendMessage(g_hwnd, WM_COMMAND, IsKeyDown(VK_SHIFT) ? ID_FILE_SAVEAS : ID_FILE_SAVEPROJECT, 0);
	m_Functions.ClearShift();
	ScheduleAction(timeGetTime() + 1000, &CSurf_Faderport::ClearSaveLED);
}

void CSurf_Faderport::Redo() {
	m_Functions.SetBtnColour(B_REDO, 16802816);
	m_midiout->Send(BTN, B_REDO, STATE_ON, -1);
	SendMessage(g_hwnd, WM_COMMAND, IDC_EDIT_REDO, 0);
	m_Functions.ClearShift();
	ScheduleAction(timeGetTime() + 1000, &CSurf_Faderport::ClearRedoLED);
}

void CSurf_Faderport::Undo() {
	m_Functions.SetBtnColour(B_UNDO, 16802816);
	m_midiout->Send(BTN, B_UNDO, STATE_ON, -1);
	SendMessage(g_hwnd, WM_COMMAND, IDC_EDIT_UNDO, 0);
	m_Functions.ClearShift();
	ScheduleAction(timeGetTime() + 3000, &CSurf_Faderport::ClearUndoLED);
}

void CSurf_Faderport::User1() {}
void CSurf_Faderport::User2() {}
void CSurf_Faderport::User3() {}

bool CSurf_Faderport::OnAutoMode(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt) || m_surfaceState.GetShift()) return false;

	int midi_code = evt->midi_message[1];
	int mode = 0;

	switch (midi_code) {
	case B_AUTO_READ:
		mode = AUTO_MODE_READ;
		break;
	case B_AUTO_WRITE:
		mode = AUTO_MODE_WRITE;
		break;
	case B_AUTO_TRIM:
		mode = AUTO_MODE_TRIM;
		break;
	case B_AUTO_TOUCH:
		mode = AUTO_MODE_TOUCH;
		break;
	case B_AUTO_LATCH:
		mode = AUTO_MODE_LATCH;
		break;
	default:
		return false;
	}

	// Set only selected tracks
	SetAutomationMode(mode, true);

	return true;
}

bool CSurf_Faderport::OnTransportEvt(MIDI_event_t* evt) {
	if (!m_Functions.isBtn(evt)) return false;

	switch (evt->midi_message[1]) {
	case B_RECORD:
		CSurf_OnRecord();
		return true;
	case B_PLAY:
		CSurf_OnPlay();
		return true;
	case B_STOP:
		CSurf_OnStop();
		return true;
	case B_RR:
		SendMessage(g_hwnd, WM_COMMAND, ID_MARKER_PREV, 0);
		return true;
	case B_FF:
		SendMessage(g_hwnd, WM_COMMAND, ID_MARKER_NEXT, 0);
		return true;
	case B_CYCLE:
		SendMessage(g_hwnd, WM_COMMAND, IDC_REPEAT, 0);
		return true;
	case B_CLICK:
		SendMessage(g_hwnd, WM_COMMAND, ID_METRONOME, 0);
		return true;
	}

	return false;
}

void CSurf_Faderport::PrevNext(int direction) {
	int surface_id = m_surfaceState.GetSurfaceId();

	m_surfaceState.SetDoPrevNext(direction);
}

void CSurf_Faderport::ClearPrevNextLED() {
	m_midiout->Send(BTN, B_NEXT, STATE_OFF, -1);
	m_midiout->Send(BTN, B_PREV, STATE_OFF, -1);
}

// Surface event functions
void CSurf_Faderport::SetAudioLED() {
	m_midiout->Send(BTN, B_AUDIO, STATE_ON, -1);
	m_midiout->Send(BTN, B_BUS, STATE_OFF, -1);
	m_midiout->Send(BTN, B_VCA, STATE_OFF, -1);
	m_midiout->Send(BTN, B_VI, STATE_OFF, -1);
	m_midiout->Send(BTN, B_ALL, STATE_OFF, -1);

	m_Functions.SetBtnColour(B_BUS, 33554431);
}

void CSurf_Faderport::SetBusLED() {
	m_midiout->Send(BTN, B_AUDIO, STATE_OFF, -1);
	m_midiout->Send(BTN, B_BUS, STATE_ON, -1);
	m_midiout->Send(BTN, B_VCA, STATE_OFF, -1);
	m_midiout->Send(BTN, B_VI, STATE_OFF, -1);
	m_midiout->Send(BTN, B_ALL, STATE_OFF, -1);

	m_Functions.SetBtnColour(B_BUS, 33554431);
}

void CSurf_Faderport::SetVCALED() {
	m_midiout->Send(BTN, B_AUDIO, STATE_OFF, -1);
	m_midiout->Send(BTN, B_BUS, STATE_OFF, -1);
	m_midiout->Send(BTN, B_VCA, STATE_ON, -1);
	m_midiout->Send(BTN, B_VI, STATE_OFF, -1);
	m_midiout->Send(BTN, B_ALL, STATE_OFF, -1);
	m_Functions.SetBtnColour(B_VCA, 33554431);
}

void CSurf_Faderport::SetVILED() {
	m_midiout->Send(BTN, B_AUDIO, STATE_OFF, -1);
	m_midiout->Send(BTN, B_BUS, STATE_OFF, -1);
	m_midiout->Send(BTN, B_VCA, STATE_OFF, -1);
	m_midiout->Send(BTN, B_VI, STATE_ON, -1);
	m_midiout->Send(BTN, B_ALL, STATE_OFF, -1);
	m_Functions.SetBtnColour(B_VI, 33554431);
}

void CSurf_Faderport::SetAllLED() {
	m_midiout->Send(BTN, B_AUDIO, STATE_OFF, -1);
	m_midiout->Send(BTN, B_BUS, STATE_OFF, -1);
	m_midiout->Send(BTN, B_VCA, STATE_OFF, -1);
	m_midiout->Send(BTN, B_VI, STATE_OFF, -1);
	m_midiout->Send(BTN, B_ALL, STATE_ON, -1);
	m_Functions.SetBtnColour(B_ALL, 33554431);
}

void CSurf_Faderport::SetLinkLED() {
	m_surfaceState.ToggleLink();
	m_midiout->Send(BTN, B_LINK, m_surfaceState.GetLink() ? STATE_ON : STATE_OFF, -1);
	m_Functions.SetBtnColour(B_LINK, 33554431);
	//ClearCaches();
}

void CSurf_Faderport::SetMacroLED() {
	m_midiout->Send(BTN, B_MACRO, m_surfaceState.GetSpread() ? STATE_ON : STATE_OFF, -1);
	m_Functions.SetBtnColour(B_MACRO, 33554431);
	ClearCaches();
}

void CSurf_Faderport::MuteSelectedTrack(int midi_code, bool doubleClick) {
	int track_id = m_Functions.SurfaceMuteBtnToReaperTrackId(midi_code);

	MediaTrack* track = g_vs_currentview->media_track[track_id];

	(doubleClick && track) ? MuteAllTracks(0) : false;
	track ? CSurf_SetSurfaceMute(track, CSurf_OnMuteChange(track, -1), NULL) : false;
}

void CSurf_Faderport::SoloSelectedTrack(int midi_code, bool doubleClick) {
	int track_id = m_Functions.SurfaceSoloBtnToReaperTrackId(midi_code);

	MediaTrack* track = g_vs_currentview->media_track[track_id];

	if (doubleClick && track) SoloAllTracks(0);

	track ? CSurf_SetSurfaceSolo(track, CSurf_OnSoloChange(track, -1), NULL) : false;
}

void CSurf_Faderport::SoloClearAll(int midi_code) {
	m_midiout->Send(BTN, B_SOLO_CLEAR, STATE_OFF, -1);
	SoloAllTracks(0);
}

void CSurf_Faderport::MuteClearAll(int midi_code) {
	m_midiout->Send(BTN, B_MUTE_CLEAR, STATE_OFF, -1);
	MuteAllTracks(false);
}

void CSurf_Faderport::SetRecordArm(MediaTrack* track) {
	if (!track) return;

	int state = CSurf_OnRecArmChange(track, -1);
	CSurf_SetSurfaceRecArm(track, state, NULL);
}

// Select each track
void CSurf_Faderport::SelectTrack(int midi_code) {
	int trackId = m_Functions.SurfaceSelectBtnToReaperTrackId(midi_code);
	MediaTrack* track = g_vs_currentview->media_track[trackId];

	if (!track) return;

	// Arm the track instead of select
	if (m_surfaceState.GetArm()) {
		SetRecordArm(track);
		return;
	}

	// Select the track in Reaper
	CSurf_OnSelectedChange(track, -1);
}

// Doubleclick event
// Select only this track
void CSurf_Faderport::SelectTrackDoubleClick(int midi_code) {
	if (m_surfaceState.GetArm()) return;

	int track_id = m_Functions.SurfaceSelectBtnToReaperTrackId(midi_code);
	MediaTrack* track = g_vs_currentview->media_track[track_id];

	if (!track) return;

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

	// Select the track in Reaper
	CSurf_OnSelectedChange(track, 1);
}

// Toggle MCP / TCP
void CSurf_Faderport::SetMCPTCPView() {
	m_surfaceState.ToggleMCPMode();
	m_midiout->Send(BTN, B_ALL, !m_surfaceState.GetMCPMode() ? STATE_ON : STATE_OFF, -1);

	// Clear the surface caches because we are loading a new view
	ClearCaches();
}

// Set arm
void CSurf_Faderport::SetArmState() {
	// Clear select button caches since arming changes the state
	ClearSelectButtonCache();

	if (CountTracks(0) < 1) return;

	m_surfaceState.ToggleArm();
	m_midiout->Send(BTN, B_ARM, m_surfaceState.GetArm() ? STATE_ON : STATE_OFF, -1);
	m_surfaceState.GetArm() ? m_Functions.ClearSelectBtns() : false;
}

void CSurf_Faderport::ClearArmState() {
	// Clear select button caches since de-arming changes the state
	ClearSelectButtonCache();

	if (CountTracks(0) < 1) return;

	m_surfaceState.SetArm(0);
	m_midiout->Send(BTN, B_ARM, STATE_OFF, -1);
	ClearAllRecArmed();
}

void CSurf_Faderport::SetMasterState() {
	m_surfaceState.ToggleMaster();
	m_midiout->Send(BTN, B_MASTER, m_surfaceState.GetMaster() ? STATE_ON : STATE_OFF, -1);
}

void CSurf_Faderport::SetShiftState() {
	m_surfaceState.ToggleShift();
	m_midiout->Send(BTN, B_SHIFTL, m_surfaceState.GetShift() ? STATE_ON : STATE_OFF, -1);
	m_midiout->Send(BTN, B_SHIFTR, m_surfaceState.GetShift() ? STATE_ON : STATE_OFF, -1);
}

void CSurf_Faderport::ClearSaveLED() {
	m_midiout->Send(BTN, B_SAVE, 0, -1);
}

void CSurf_Faderport::ClearUndoLED() {
	m_midiout->Send(BTN, B_UNDO, 0, -1);
}

void CSurf_Faderport::ClearRedoLED() {
	m_midiout->Send(BTN, B_REDO, 0, -1);
}

void CSurf_Faderport::SetPanState() {
	g_fader_pan = !g_fader_pan;
	m_midiout->Send(BTN, B_PAN, g_fader_pan ? 1 : 0, -1);
	ClearCaches();
}

void CSurf_Faderport::SetSendsState() {
	g_send_state = !g_send_state;
}

void CSurf_Faderport::SetAudioViewState() {
	m_surfaceState.SetAudioView(1);
	m_surfaceState.SetBusView(0);
	m_surfaceState.SetVCAView(0);
	m_surfaceState.SetAllView(0);
	m_surfaceState.SetVIView(0);
	ClearCaches();
}

void CSurf_Faderport::SetBusViewState() {
	m_surfaceState.SetAudioView(0);
	m_surfaceState.SetBusView(1);
	m_surfaceState.SetVCAView(0);
	m_surfaceState.SetAllView(0);
	m_surfaceState.SetVIView(0);
	ClearCaches();
}

void CSurf_Faderport::SetVCAViewState() {
	m_surfaceState.SetAudioView(0);
	m_surfaceState.SetBusView(0);
	m_surfaceState.SetVCAView(1);
	m_surfaceState.SetAllView(0);
	m_surfaceState.SetVIView(0);
	ClearCaches();
}

void CSurf_Faderport::SetAllViewState() {
	m_surfaceState.SetAudioView(0);
	m_surfaceState.SetBusView(0);
	m_surfaceState.SetVCAView(0);
	m_surfaceState.SetAllView(1);
	m_surfaceState.SetVIView(0);
	ClearCaches();
}

void CSurf_Faderport::SetVIViewState() {
	m_surfaceState.SetAudioView(0);
	m_surfaceState.SetBusView(0);
	m_surfaceState.SetVCAView(0);
	m_surfaceState.SetAllView(0);
	m_surfaceState.SetVIView(1);
	ClearCaches();
}

void CSurf_Faderport::SetChannelState() {
	m_surfaceState.SetChannel(1);
	m_surfaceState.SetBank(0);
	ClearCaches();
}

void CSurf_Faderport::SetBankState() {
	m_surfaceState.SetBank(1);
	m_surfaceState.SetChannel(0);
	ClearCaches();
}

#pragma endregion midi

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma region run
// Main loop
void CSurf_Faderport::Run() {
	if (!m_midiin || !m_midiout) return;

	DWORD now = timeGetTime();
	// Run processes continously so limit to the cycle given (15/30hz)
	if (now >= m_frameupd_lastrun + (1000 / max((*g_config_csurf_rate), 1)) || now < m_frameupd_lastrun - 250)
	{
		m_frameupd_lastrun = now;

		PrevNextCheck();
		UpdateVirtualLayoutViews();

		// Run any scheduled events
		while (m_schedule && now >= m_schedule->time) {
			ScheduledAction* action = m_schedule;
			m_schedule = m_schedule->next;
			(this->*(action->func))();
			delete action;
		}

		int m_number_of_tracks_last = CSurf_NumTracks(m_surfaceState.GetMCPMode());

		// Polling is much more flexible than using the Reaper events
		// Caching is used to only do something when a change is made else the surface gets overwhelmed with midi data
		SyncReaperToSurface(now);

		// Collect and process midi events
		m_midiin->SwapBufs(timeGetTime());
		int l = 0;
		MIDI_eventlist* list = m_midiin->GetReadBuf();
		MIDI_event_t* evts;
		while ((evts = list->EnumItems(&l))) OnMidiEvent(evts);
	}
}

void CSurf_Faderport::PrevNextCheck() {
	int surface_id = m_surfaceState.GetSurfaceId();

	//int direction = g_doprevnext[surface_id];
	int direction = m_surfaceState.GetDoPrevNext();
	if (direction > -1) {
		DoPrevNext(direction);
		m_surfaceState.SetDoPrevNext(-1);
		//g_doprevnext[surface_id] = -1;
	}
}

void CSurf_Faderport::DoPrevNext(int direction) {
	int surface_id = m_surfaceState.GetSurfaceId();
	int movesize;
	int move_by_channel = !m_surfaceState.GetBank();
	int current_bankchannel_offset = m_surfaceState.GetPrevNextOffset();

	int surface_size = m_surfaceState.GetSurfaceSize();
	int track_offset = m_surfaceState.GetTrackOffset();

	move_by_channel ? movesize = 1 : movesize = surface_size;

	int number_reaper_tracks = CSurf_NumTracks(m_surfaceState.GetMCPMode()) - m_surfaceState.GetTrackOffset();

	if (m_surfaceState.GetAudioView() == 1) {
		number_reaper_tracks = CountAudioTracks();
	}

	if (m_surfaceState.GetVIView() == 1) {
		number_reaper_tracks = CountVITracks();
	}

	if (m_surfaceState.GetBusView() == 1) {
		number_reaper_tracks = CountBusTracks();
	}

	if (m_surfaceState.GetVCAView() == 1) {
		number_reaper_tracks = CountVCATracks();
	}

	if (number_reaper_tracks <= surface_size) return;

	// Find direction
	if (direction == 1) // right
	{
		m_midiout->Send(BTN, B_NEXT, STATE_ON, -1);

		current_bankchannel_offset += movesize;

		// when moving by bank do not exceed the maximum number of tracks in reaper
		if (!move_by_channel && (current_bankchannel_offset + surface_size > number_reaper_tracks)) {
			current_bankchannel_offset = (number_reaper_tracks - surface_size);
		}

		// when moving by channel do not exceed the maximum number of tracks in reaper
		if (move_by_channel && (current_bankchannel_offset + surface_size > number_reaper_tracks)) {
			current_bankchannel_offset -= 1;
		}
	}
	else // left
	{
		m_midiout->Send(BTN, B_PREV, STATE_ON, -1);

		current_bankchannel_offset -= movesize;

		// when moving by channel do not go beyond the first track
		if (current_bankchannel_offset - track_offset < 0) {
			current_bankchannel_offset = 0;
		}
	}

	m_surfaceState.SetPrevNextOffset(current_bankchannel_offset);
	// always clear the surface track caches whenever the track positions get updated
	ClearCaches();

	ScheduleAction(timeGetTime() + 150, &CSurf_Faderport::ClearPrevNextLED);
}

// Status from Reaper to the Surface controller
void CSurf_Faderport::SyncReaperToSurface(DWORD now) {

	//Get states and turn on/off LEDs
	// These functions act as caches to stop the Faderport updating its state each cycle
	CacheRepeatState();
	CacheMetronomeState();
	CacheAudioViewState();
	CacheBusViewState();
	CacheVCAViewState();
	CacheVIViewState();
	CacheAllViewState();
	CacheChannelState();
	CacheBankState();


	int VU_BOTTOM = 70;
	double decay = 0.0;

	if (m_mcu_meter_lastrun)
	{
		decay = VU_BOTTOM * (double)(now - m_mcu_meter_lastrun) / (1.4 * 1000.0);   // they claim 1.8s for falloff but we'll underestimate
	}

	m_mcu_meter_lastrun = now;

	for (int surface_displayid = 0; surface_displayid < m_surfaceState.GetSurfaceSize(); surface_displayid++) {
		MediaTrack* track = g_vs_currentview->media_track[surface_displayid];

		// Clean up if no track is found
		if (!track) {
			// Cache removed surface displayid
			CleanUpTrackDisplay(surface_displayid);
			continue;
		}

		//ClearTrackTitleCache();
		m_surfacedisplayid_removed[surface_displayid] = 0;

		double pan = GetMediaTrackInfo_Value(track, "D_PAN");
		double volume = GetMediaTrackInfo_Value(track, "D_VOL");
		SetSurfaceFader(surface_displayid, volume, pan);

		// Update the pan display
		UpdateSurfaceValueBar(surface_displayid, pan);

		// Fix this when trackstate manager is done
		if (m_valuebarmode_lastpos[surface_displayid] != MODE_NORMAL) {
			m_Functions.ResetValueBar(surface_displayid, MODE_NORMAL);
			m_valuebarmode_lastpos[surface_displayid] = MODE_NORMAL;
		}

		int isSelected = (int)GetMediaTrackInfo_Value(track, "I_SELECTED");
		int armed = (int)GetMediaTrackInfo_Value(track, "I_RECARM");
		!m_surfaceState.GetArm() ? SetSelectLED(track, surface_displayid, isSelected) : SetSelectArmedLED(surface_displayid, armed);

		// Light mute button for selected tracks
		int mute = (int)GetMediaTrackInfo_Value(track, "B_MUTE");
		SetMuteLED(surface_displayid, mute);

		// Light solo button for selected tracks
		int solo = (int)GetMediaTrackInfo_Value(track, "I_SOLO");
		SetSoloLED(surface_displayid, solo);

		char* title = (char*)GetSetMediaTrackInfo(track, "P_NAME", NULL);
		int trackNumber = (int)GetMediaTrackInfo_Value(track, "IP_TRACKNUMBER");
		SetSurfaceTrackTitle(surface_displayid, title, trackNumber);

		int trackColour = !m_surfaceState.GetArm() ? (int)GetMediaTrackInfo_Value(track, "I_CUSTOMCOLOR") : 16777471;
		SetSurfaceTrackColour(surface_displayid, trackColour);

		int v = CalculateMeter(track, surface_displayid, VU_BOTTOM, decay);
		v != -1 ? UpdateSurfaceMeter(surface_displayid, v) : false;
	}
}

void CSurf_Faderport::CleanUpTrackDisplay(int surface_displayid) {
	// Cache removed surface displayid
	if (m_surfacedisplayid_removed[surface_displayid] == 1) return;

	m_surfacedisplayid_removed[surface_displayid] = 1;
	m_vol_lastpos[surface_displayid] = g_fader_pan ? panToInt14(0.0) : volToInt14(0.0);
	m_pan_lastpos[surface_displayid] = !g_fader_pan ? panToInt14(0.0) : volToInt14(0.0);

	m_Functions.ResetFader(surface_displayid);
	m_Functions.ClearScribbleText(surface_displayid);

	// Fix this when trackstate manager is done
	if (m_valuebarmode_lastpos[surface_displayid] != MODE_OFF) {
		m_Functions.ResetValueBar(surface_displayid, MODE_OFF);
		m_valuebarmode_lastpos[surface_displayid] = MODE_OFF;
	}

	m_Functions.ClearSoloBtn(surface_displayid);
	m_Functions.ClearMuteBtn(surface_displayid);
	m_Functions.ClearMeter(surface_displayid);
	m_Functions.ClearSelect(surface_displayid);

	ClearTrackTitleCache();
}

int CSurf_Faderport::CalculateMeter(MediaTrack* track, int surface_displayid, int VU_BOTTOM, double decay) {
	if (!(GetPlayState() & 1)) return -1;

	double pp = VAL2DB((Track_GetPeakInfo(track, 0) + Track_GetPeakInfo(track, 1)) * 0.5);

	if (m_mcu_meterpos[surface_displayid] > -VU_BOTTOM * 2) m_mcu_meterpos[surface_displayid] -= decay;

	if (pp < m_mcu_meterpos[surface_displayid]) return -1;

	m_mcu_meterpos[surface_displayid] = pp;

	//int v = 0xd; // 0xe turns on clip indicator, 0xf turns it off
	int v = 0x0;

	if (pp < 0.0) pp < -VU_BOTTOM ? v = 0x0 : v = (int)((pp + VU_BOTTOM) * 13.0 / VU_BOTTOM) * 10;

	return v;
}

void CSurf_Faderport::CacheAudioViewState() {

	int state = m_surfaceState.GetAudioView();

	if (m_audio_lastpos == state) return;

	m_audio_lastpos = state;

	SetAudioLED();
}

void CSurf_Faderport::CacheBusViewState() {

	int state = m_surfaceState.GetBusView();

	if (m_bus_lastpos == state) return;

	m_bus_lastpos = state;

	SetBusLED();
}

void CSurf_Faderport::CacheVCAViewState() {
	int state = m_surfaceState.GetVCAView();

	if (m_vca_lastpos == state) return;

	m_vca_lastpos= state;

	SetVCALED();
}

void CSurf_Faderport::CacheVIViewState() {
	int state = m_surfaceState.GetVIView();

	if (m_vi_lastpos == state) return;

	m_vi_lastpos = state;

	SetVILED();
}

void CSurf_Faderport::CacheAllViewState() {
	int state = m_surfaceState.GetAllView();

	if (m_all_lastpos == state) return;

	m_all_lastpos = state;

	SetAllLED();
}

void CSurf_Faderport::CacheBankState() {
	int state = m_surfaceState.GetBank();

	if (m_bank_lastpos == state) return;

	m_bank_lastpos = state;
	SetChannelBankLED();
}

void CSurf_Faderport::CacheChannelState() {
	int state = m_surfaceState.GetChannel();

	if (m_channel_lastpos == state) return;

	m_channel_lastpos = state;
	SetChannelBankLED();
}

void CSurf_Faderport::CacheRepeatState() {
	int state = (int)GetSetRepeat(-1) == 1 ? 1 : 0;

	if (m_cycle_lastpos == state) return;

	m_cycle_lastpos = state;
	m_midiout->Send(BTN, B_CYCLE, state ? STATE_ON : STATE_OFF, -1);
}

void CSurf_Faderport::CacheMetronomeState() {
	int sz;
	int m_metronome_offset = projectconfig_var_getoffs("projmetroen", &sz);

	if (sz != 4) m_metronome_offset = 0;

	if (m_metronome_offset)
	{
		int* metro_en = (int*)projectconfig_var_addr(NULL, m_metronome_offset);

		if (metro_en) {
			int a = *metro_en;

			if (m_metronome_lastpos == a) return;

			m_metronome_lastpos = a;
			m_midiout->Send(BTN, B_CLICK, a & 1 ? STATE_ON : STATE_OFF, -1);
		}
	}
}

void CSurf_Faderport::SetSurfaceTrackColour(int surface_displayid, int colour) {
	if (m_trackcolour_lastpos[surface_displayid] == colour) return;

	m_trackcolour_lastpos[surface_displayid] = colour;

	m_Functions.SetBtnColour(m_Functions.SurfaceDisplayIdToSelectBtn(surface_displayid), colour);
}

void CSurf_Faderport::SetSurfaceFader(int surface_displayid, double volume, double pan) {
	int volint = volToInt14(volume);
	int panint = actualFaderPanToInt14(pan);
	unsigned char panch = actualFaderPanToChar(pan);

	// Set the fader to the pan state
	if (g_fader_pan)
	{
		if (m_pan_lastpos[surface_displayid] == panch) return;

		m_pan_lastpos[surface_displayid] = panch;

		// Position the fader
		m_midiout->Send(FADER + (surface_displayid), panint & STATE_ON, panint & STATE_ON, -1);

		return;
	}

	if (m_vol_lastpos[surface_displayid] == volint) return;

	m_vol_lastpos[surface_displayid] = volint;
	m_midiout->Send(FADER + (surface_displayid), volint & 0x7F, (volint >> 7) & 0x7F, -1);
}

// Update the displays value bar
void CSurf_Faderport::UpdateSurfaceValueBar(int surface_displayid, double pan) {
	unsigned char panch = panToChar(pan);

	if (m_valuebar_lastpos[surface_displayid] == panch) return;

	m_valuebar_lastpos[surface_displayid] = panch;

	int panint = 127 - panToInt14(pan); // change direction
	int panhex = (panint >> 7) & 0x7F;

	// set value bar to far right if 100% right-pan
	panhex == 0 ? panhex = 0x7F : false;

	// Update the value bar
	surface_displayid < 8 ? m_midiout->Send(VALUEBAR, 0x30 + (surface_displayid), panhex, 0) :
		m_midiout->Send(VALUEBAR, 0x40 + (surface_displayid - 8), panhex, 0);
}

void CSurf_Faderport::SetSurfaceTrackTitle(int surface_displayid, std::string title, int trackId) {
	std::string sTitle = title;
	std::string tNumber = std::to_string(trackId);
	std::string combined = sTitle + tNumber;

	if (m_tracktitle_last[surface_displayid] == combined) return;

	m_tracktitle_last[surface_displayid] = combined;

	m_Functions.SendTextToScribble(surface_displayid, 0, 1, title);
	m_Functions.SendTextToScribble(surface_displayid, 1, 0, tNumber);
}

void CSurf_Faderport::SetSelectLED(MediaTrack* track, int surface_displayid, int selected) {
	if (m_select_lastpos[surface_displayid] == selected) return;

	selected ? SelectTrack(track) : DeselectTrack(track);

	m_select_lastpos[surface_displayid] = selected;
	m_midiout->Send(BTN, m_Functions.SurfaceDisplayIdToSelectBtn(surface_displayid), selected ? STATE_ON : STATE_OFF, -1);

	UpdateAutomationModes();
}

void CSurf_Faderport::SetSelectArmedLED(int surface_displayid, int armed) {
	if (m_arm_lastpos[surface_displayid] == armed) return;

	m_arm_lastpos[surface_displayid] = armed;
	m_midiout->Send(BTN, m_Functions.SurfaceDisplayIdToSelectBtn(surface_displayid), armed ? STATE_ON : STATE_OFF, -1);
}

void CSurf_Faderport::SetMuteLED(int surface_displayid, int mute) {
	if (m_mute_lastpos[surface_displayid] == mute) return;

	int muteBtnOffset = B_MUTE1;

	if (surface_displayid > 7) muteBtnOffset = B_MUTE9 - 0x08;

	m_mute_lastpos[surface_displayid] = mute;
	m_midiout->Send(BTN, muteBtnOffset + surface_displayid, mute ? STATE_ON : STATE_OFF, -1);
	m_Functions.AnyMute() ? m_midiout->Send(BTN, B_MUTE_CLEAR, STATE_ON, -1) : m_midiout->Send(BTN, B_MUTE_CLEAR, STATE_OFF, -1);
}

void CSurf_Faderport::SetSoloLED(int surface_displayid, int solo) {
	if (m_solo_lastpos[surface_displayid] == solo) return;

	m_solo_lastpos[surface_displayid] = solo;

	int btn_code = B_SOLO1 + (surface_displayid);

	// rules for buttons 9-16
	if (surface_displayid > 7 && surface_displayid < 14) btn_code = 0x48 + (surface_displayid);

	if (surface_displayid == 11) btn_code = B_SOLO12;

	if (surface_displayid == 14) btn_code = B_SOLO15;

	if (surface_displayid == 15) btn_code = B_SOLO16;

	m_midiout->Send(BTN, btn_code, solo ? STATE_ON : 0, -1); // STATE_BLINK ?

	// Set Solo clear depending on if any tracks are soloed or not
	m_Functions.AnySolo() ? m_midiout->Send(BTN, B_SOLO_CLEAR, STATE_ON, -1) : m_midiout->Send(BTN, B_SOLO_CLEAR, STATE_OFF, -1);
}

void  CSurf_Faderport::SetChannelBankLED() {
	m_midiout->Send(BTN, B_CHANNEL, m_surfaceState.GetChannel() ? STATE_ON : STATE_OFF, -1);
	m_midiout->Send(BTN, B_BANK, m_surfaceState.GetBank() ? STATE_ON : STATE_OFF, -1);
}

// Update the meterbar on the display
void CSurf_Faderport::UpdateSurfaceMeter(int surface_displayid, int v) {
	(surface_displayid > 7) ? m_midiout->Send(PEAK_METER_9_16 + (surface_displayid & 7), v, 0, -1) : m_midiout->Send(PEAK_METER_1_8 + surface_displayid, v, 0, -1);
}

void CSurf_Faderport::UpdateAutomationModes() {
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

	m_midiout->Send(BTN, B_AUTO_READ, modes[AUTO_MODE_READ] ? (multi ? 1 : STATE_ON) : 0, -1);
	m_midiout->Send(BTN, B_AUTO_WRITE, modes[AUTO_MODE_WRITE] ? (multi ? 1 : STATE_ON) : 0, -1);
	m_midiout->Send(BTN, B_AUTO_TRIM, modes[AUTO_MODE_TRIM] ? (multi ? 1 : STATE_ON) : 0, -1);
	m_midiout->Send(BTN, B_AUTO_TOUCH, modes[AUTO_MODE_TOUCH] ? (multi ? 1 : STATE_ON) : 0, -1);
	m_midiout->Send(BTN, B_AUTO_LATCH, modes[AUTO_MODE_LATCH] ? (multi ? 1 : STATE_ON) : 0, -1);
}

void CSurf_Faderport::SelectTrack(MediaTrack* track) {
	if (m_midiout == NULL) return;
	const GUID* guid = GetTrackGUID(track);

	// Empty list, start new list
	if (m_selected_tracks == NULL) {
		m_selected_tracks = new SelectedTrack(track);
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
	i->next = new SelectedTrack(track);
}

void CSurf_Faderport::DeselectTrack(MediaTrack* track) {
	if (m_midiout == NULL) return;
	const GUID* guid = GetTrackGUID(track);

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

// Link - move the bank on the Faderport within the track selected from Reaper
void CSurf_Faderport::SetTrackSelection(MediaTrack* track) {
	if (!m_surfaceState.GetLink()) return;

	int trackId = CSurf_TrackToID(track, m_surfaceState.GetMCPMode());

	MoveToBank(trackId);
}

void CSurf_Faderport::MoveToBank(int track_offset) {
	int number_reaper_tracks = CSurf_NumTracks(m_surfaceState.GetMCPMode()) - m_surfaceState.GetTrackOffset();
	int selectedTrackBank = GetBankNumberFromTrackId(track_offset);
	int surface_size = m_surfaceState.GetSurfaceSize(); 

	if (m_surfaceState.GetAudioView() == 1) {
		number_reaper_tracks = CountAudioTracks();
	}

	if (m_surfaceState.GetVIView() == 1) {
		number_reaper_tracks = CountVITracks();
	}

	if (m_surfaceState.GetBusView() == 1) {
		number_reaper_tracks = CountBusTracks();
	}

	if (m_surfaceState.GetVCAView() == 1) {
		number_reaper_tracks = CountVCATracks();
	}

	if (number_reaper_tracks <= surface_size) return;
	
	int offset = 0;
 	
	if (( track_offset + (surface_size) > number_reaper_tracks)) {
		offset = number_reaper_tracks - surface_size;
	}
	else {
		offset = surface_size * selectedTrackBank;
	}
 
 	m_surfaceState.SetPrevNextOffset(offset);

	ClearCaches();
}


int CSurf_Faderport::GetCurrentBankNumber() {
	return (int)(m_Functions.GetCurrentSurfaceOffset() / m_surfaceState.GetSurfaceSize());
}

int CSurf_Faderport::GetBankNumberFromTrackId(int track_offset) {
	return ((track_offset - (m_surfaceState.GetTrackOffset() + 1)) / m_surfaceState.GetSurfaceSize());
}
#pragma endregion run

#pragma region events
// Reaper API Events (Do something on the surface when something happens in Reaper)
// Most of the standard events have been replaced with polling. These are the ones that are left
// --------------------------------------------------------------------------------
void CSurf_Faderport::SetTrackListChange()
{
	//
	int number_of_tracks = CSurf_NumTracks(m_surfaceState.GetMCPMode());

	// Clear caches and prevnext offset
	if (number_of_tracks < g_number_of_tracks_last) {
		int difference = m_surfaceState.GetPrevNextOffset() - g_number_of_tracks_last - number_of_tracks;
		m_surfaceState.SetPrevNextOffset(difference);
		g_number_of_tracks_last = number_of_tracks;
		ClearCaches();
	}
}

bool CSurf_Faderport::GetTouchState(MediaTrack* track, int isPan)
{
	int surface_displayid = m_Functions.GetSurfaceDisplayIdFromReaperMediaTrack(track);

	if (surface_displayid < 0 || surface_displayid > m_surfaceState.GetSurfaceSize()) return false;

	DWORD now = timeGetTime();

	if (g_fader_pan == 0)
	{
		return (m_pan_lasttouch[surface_displayid] == 1 || (now < m_pan_lasttouch[surface_displayid] + 3000 && now >= m_pan_lasttouch[surface_displayid] - 1000)); // fake touch, go for 3s after last movement
	}

	if (!m_fader_touchstate[surface_displayid] && m_fader_lasttouch[surface_displayid] && m_fader_lasttouch[surface_displayid] != 0xffffffff)
	{
		return (now < m_fader_lasttouch[surface_displayid] + 3000 && now >= m_fader_lasttouch[surface_displayid] - 1000);
	}

	// !! converts to bool
	return !!m_fader_touchstate[surface_displayid];
}

// Send the play status
void CSurf_Faderport::SetPlayState(bool play, bool pause, bool rec) {
	if (m_midiout == NULL) return;

	m_midiout->Send(BTN, B_RECORD, rec ? STATE_ON : 0, -1);
	m_midiout->Send(BTN, B_PLAY, play || pause ? STATE_ON : 0, -1);
	m_midiout->Send(BTN, B_STOP, !play ? STATE_ON : 0, -1);
}

// Automation modes
void CSurf_Faderport::SetAutoMode(int mode)
{
	UpdateAutomationModes();
}

// Set the current track to the selected track
void CSurf_Faderport::OnTrackSelection(MediaTrack* track) {
	SetTrackSelection(track);
}

int CSurf_Faderport::CountAudioTracks() {
	int max_tracks = CSurf_NumTracks(m_surfaceState.GetMCPMode()) - m_surfaceState.GetPrevNextOffset();

	int count = 0;
	for (int i = 0; i < max_tracks; i++) {
		MediaTrack* track = CSurf_TrackFromID(i, m_surfaceState.GetMCPMode());

		if (isAudioTrack(track)) count++;
	}

	return count;
}

int CSurf_Faderport::CountVITracks() {
	int max_tracks = CSurf_NumTracks(m_surfaceState.GetMCPMode()) - m_surfaceState.GetPrevNextOffset();

	int count = 0;
	for (int i = 0; i < max_tracks; i++) {
		MediaTrack* track = CSurf_TrackFromID(i, m_surfaceState.GetMCPMode());

		if (isVITrack(track)) count++;
	}

	return count;
}

int CSurf_Faderport::CountBusTracks() {
	int max_tracks = CSurf_NumTracks(m_surfaceState.GetMCPMode()) - m_surfaceState.GetPrevNextOffset();

	int count = 0;
	for (int i = 0; i < max_tracks; i++) {
		MediaTrack* track = CSurf_TrackFromID(i, m_surfaceState.GetMCPMode());

		if (isBusTrack(track)) count++;
	}

	return count;
}

int CSurf_Faderport::CountVCATracks() {
	int max_tracks = CSurf_NumTracks(m_surfaceState.GetMCPMode()) - m_surfaceState.GetPrevNextOffset();

	int count = 0;
	for (int i = 0; i < max_tracks; i++) {
		MediaTrack* track = CSurf_TrackFromID(i, m_surfaceState.GetMCPMode());

		if (track != NULL && isVCATrack(track)) count++;
	}

	return count;
}

void  CSurf_Faderport::UpdateVirtualLayoutViews() {
	int track_id_audio = 0;
	int track_id_bus = 0;
	int track_id_vca = 0;
	int track_id_midi = 0;
	int track_id_vi = 0;
	int track_id_all = 0;
	int current_reaper_trackId = 0;
	int max_tracks = CSurf_NumTracks(m_surfaceState.GetMCPMode()) - m_surfaceState.GetPrevNextOffset();

	g_vs_audioview = {};
	g_vs_viview = {};
	g_vs_busview = {};
	g_vs_vcaview = {};
	g_vs_allview = {};
	
	int prevnext_offset = m_surfaceState.GetPrevNextOffset();

	while (current_reaper_trackId < max_tracks) {
		current_reaper_trackId++;

		MediaTrack* track = CSurf_TrackFromID(prevnext_offset + current_reaper_trackId, m_surfaceState.GetMCPMode());

		if (!track) continue;

		int trackNumber = (int)GetMediaTrackInfo_Value(track, "IP_TRACKNUMBER");
		char* title = (char*)GetSetMediaTrackInfo(track, "P_NAME", NULL);

		if (!title) continue;

		// Bus view - use the bus prefix from config
		if (isAudioTrack(track)) {
			if (track_id_midi > 15) continue;

			g_vs_audioview.media_track[track_id_audio] = track;
			g_vs_audioview.media_track_last[track_id_audio] = track;
			g_vs_audioview.media_track_number[track_id_audio] = trackNumber;
			g_vs_audioview.display_number[track_id_audio] = track_id_audio;
			g_vs_audioview.number_of_tracks += 1;
			track_id_audio++;
		}else if (isBusTrack(track)) {
			if (track_id_bus > 15) continue;

			g_vs_busview.media_track[track_id_bus] = track;
			g_vs_busview.media_track_last[track_id_bus] = track;
			g_vs_busview.media_track_number[track_id_bus] = trackNumber;
			g_vs_busview.display_number[track_id_bus] = track_id_bus;
			g_vs_busview.number_of_tracks += 1;
			track_id_bus++;
		}// Vca view
		else if (isVCATrack(track)) {
			if (track_id_vca > 15) continue;

			g_vs_vcaview.media_track[track_id_vca] = track;
			g_vs_vcaview.media_track_last[track_id_vca] = track;
			g_vs_vcaview.media_track_number[track_id_vca] = trackNumber;
			g_vs_vcaview.display_number[track_id_vca] = track_id_vca;
			g_vs_vcaview.number_of_tracks += 1;
			track_id_vca++;
		}
		else if (isVITrack(track)) {
			if (track_id_vi > 15) continue;

			g_vs_viview.media_track[track_id_vi] = track;
			g_vs_viview.media_track_last[track_id_vi] = track;
			g_vs_viview.media_track_number[track_id_vi] = trackNumber;
			g_vs_viview.display_number[track_id_vi] = track_id_vi;
			g_vs_viview.number_of_tracks += 1;
			track_id_vi++;
		}
		else { // All view vsurface
			if (track_id_all > 15) continue;
			g_vs_allview.media_track[track_id_all] = track;
			g_vs_allview.media_track_last[track_id_all] = track;
			g_vs_allview.media_track_number[track_id_all] = trackNumber;
			g_vs_allview.display_number[track_id_all] = track_id_all;
			g_vs_allview.number_of_tracks += 1;
			track_id_all++;
		}
	}

	g_vs_currentview = &g_vs_allview;

	if (m_surfaceState.GetBusView()) {
		g_vs_currentview = &g_vs_busview;
	}

	if (m_surfaceState.GetVCAView()) {
		g_vs_currentview = &g_vs_vcaview;
	}

	if (m_surfaceState.GetAudioView()) {
		g_vs_currentview = &g_vs_audioview;
	}

	if (m_surfaceState.GetVIView()) {
		g_vs_currentview = &g_vs_viview;
	}
};

bool CSurf_Faderport::isBusTrack(MediaTrack* track) {
	std::string prefix = m_surfaceState.GetBusPrefix();

	char* title = (char*)GetSetMediaTrackInfo(track, "P_NAME", NULL);

	if (track == NULL || title == NULL) return false;

	std::string t = title;
	return t.find(prefix) != std::string::npos;
}

bool CSurf_Faderport::isVCATrack(MediaTrack* track) {

	int vca_l_lo = GetSetTrackGroupMembership(track, "VOLUME_VCA_LEAD", 0, 0);
	int vca_l_hi = GetSetTrackGroupMembershipHigh(track, "VOLUME_VCA_LEAD", 0, 0);

	return vca_l_lo > 0 || vca_l_hi > 0;
}

bool CSurf_Faderport::isAudioTrack(MediaTrack* track) {
	std::string prefix = m_surfaceState.GetAudioPrefix();

	char* title = (char*)GetSetMediaTrackInfo(track, "P_NAME", NULL);

	if (track == NULL || title == NULL) return false;

	std::string t = title;
	return t.find(prefix) != std::string::npos;
}

bool CSurf_Faderport::isVITrack(MediaTrack* track) {
	std::string prefix = m_surfaceState.GetVIPrefix();

	char* title = (char*)GetSetMediaTrackInfo(track, "P_NAME", NULL);

	if (track == NULL || title == NULL) return false;

	std::string t = title;
	return t.find(prefix) != std::string::npos;
}

// End Reaper events
// --------------------------------------------------------------------------------
#pragma endregion events

#pragma region init
// Constructor/Initialise
CSurf_Faderport::CSurf_Faderport(int indev, int outdev, int* errStats) {
	LoadConfig("config.txt");
	m_surfaceState.SetSurfaceId(0);

	int isfp8 = faderport == 8 ? 1 : 0;

	m_surfaceState.SetMCPMode(mcp_mode);
	m_surfaceState.SetIsFP8(isfp8);
	m_surfaceState.SetSurfaceSize(m_surfaceState.GetIsFP8() ? FP8_SIZE : FP16_SIZE);
	m_surfaceState.SetTrackOffset(start_track == 0 ? 0 : start_track - 1);
	m_surfaceState.SetPrevNextOffset(0);
	m_surfaceState.SetBusView(0);

	if (start_bus) {
		SetBusViewState();
	}
	else {
		SetAllViewState();
	}

	m_surfaceState.SetVCAView(0);
	m_surfaceState.SetVIView(0);
	m_surfaceState.SetAudioView(0);
	m_surfaceState.SetBank(1);
	m_surfaceState.SetChannel(0);
	m_surfaceState.SetBusPrefix(bus_prefix);
	m_surfaceState.SetVIPrefix(vi_prefix);
	m_surfaceState.SetAudioPrefix(audio_prefix);
	m_surfaceState.SetLink(follow);

	// Set global states
	g_fader_pan = 0;
	g_send_state = 0;

	int surface_id = m_surfaceState.GetSurfaceId();

	m_midi_in_dev = indev;
	m_midi_out_dev = outdev;

	// Initialise caches
	memset(m_vol_lastpos, 0xff, sizeof(m_vol_lastpos));
	memset(m_pan_lastpos, 0xff, sizeof(m_pan_lastpos));
	memset(m_pan_lasttouch, 0, sizeof(m_pan_lasttouch));
	memset(m_vol_lasttouch, 0, sizeof(m_vol_lasttouch));
	memset(m_select_lastpos, 0, sizeof(m_vol_lasttouch));
	memset(m_arm_lastpos, 0, sizeof(m_vol_lasttouch));
	memset(m_solo_lastpos, 0, sizeof(m_solo_lastpos));
	memset(m_mute_lastpos, 0, sizeof(m_mute_lastpos));
	memset(m_surfacedisplayid_removed, 0, sizeof(m_surfacedisplayid_removed));
	memset(m_trackcolour_lastpos, 0, sizeof(m_trackcolour_lastpos));

	m_mcu_meter_lastrun = 0;

	//create midi hardware access
	m_midiin = m_midi_in_dev >= 0 ? CreateMIDIInput(m_midi_in_dev) : NULL;
	m_midiout = m_midi_out_dev >= 0 ? CreateThreadedMIDIOutput(CreateMIDIOutput(m_midi_out_dev, false, NULL)) : NULL;

	if (!m_midiin || !m_midiout) return;

	if (errStats) {
		if (m_midi_in_dev >= 0 && !m_midiin) *errStats |= 1;
		if (m_midi_out_dev >= 0 && !m_midiout) *errStats |= 2;
	}

	m_midiin->start();

	m_Functions.SetStates(&m_surfaceState);
	m_Functions.SetMidiout(m_midiout);
	m_Functions.ResetBtns();
	m_Functions.ResetScribbles();
	m_Functions.ResetValueBars();
	m_Functions.ResetFaders();
	m_Functions.ClearMeters();
	m_Functions.ClearScribbleTexts();
	m_Functions.SetDefaultButtonColours();

	m_selected_tracks = NULL;
	m_schedule = NULL;


	// Set default buttons
	SetLinkLED();
}

void CSurf_Faderport::GetCurrentVirtualSurfaceView() {

	g_vs_currentview = &g_vs_allview;

	if (m_surfaceState.GetBusView()) {
		g_vs_currentview = & g_vs_busview;
	}

	if (m_surfaceState.GetVCAView()) {
		g_vs_currentview = &g_vs_vcaview;
	}

	if (m_surfaceState.GetVIView()) {
		g_vs_currentview = &g_vs_viview;
	}

	if (m_surfaceState.GetAudioView()) {
		g_vs_currentview = &g_vs_audioview;
	}
}

CSurf_Faderport::~CSurf_Faderport() {
	m_Functions.ResetBtns();
	m_Functions.ResetScribbles();
	m_Functions.ResetValueBars();
	m_Functions.ResetFaders();
	m_Functions.ClearMeters();
	m_Functions.ClearScribbleTexts();
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

void CSurf_Faderport::ClearCaches() {
	memset(m_vol_lastpos, 0xff, sizeof(m_vol_lastpos));
	memset(m_pan_lastpos, 0xff, sizeof(m_pan_lastpos));
	memset(m_solo_lastpos, 0xff, sizeof(m_solo_lastpos));
	memset(m_mute_lastpos, 0xff, sizeof(m_mute_lastpos));
	memset(m_surfacedisplayid_removed, 0xff, sizeof(m_surfacedisplayid_removed));
	memset(m_valuebarmode_lastpos, 0xff, sizeof(m_valuebarmode_lastpos));
	m_bank_lastpos = 0;
	m_bus_lastpos = 0;
	m_vca_lastpos = 0;
	m_audio_lastpos = 0;
	m_vi_lastpos = 0;
	m_all_lastpos = 0;

	ClearSelectButtonCache();
	ClearTrackTitleCache();
}

void CSurf_Faderport::ClearSelectButtonCache() {
	memset(m_select_lastpos, 0xff, sizeof(m_select_lastpos));
	memset(m_trackcolour_lastpos, 0xff, sizeof(m_trackcolour_lastpos));
	memset(m_arm_lastpos, 0xff, sizeof(m_arm_lastpos));
}

void CSurf_Faderport::ClearTrackTitleCache() {
	for (int i = 0; i < m_surfaceState.GetSurfaceSize(); i++) {
		m_tracktitle_last[i] = "";
	}
}

const char* CSurf_Faderport::GetTypeString() { return "FADERPORTNATIVE"; }

const char* CSurf_Faderport::GetDescString() {
	descspace.Set("Presonus Faderport 16/8 Native");
	char tmp[512];
	sprintf(tmp, " (dev %d,%d)", m_midi_in_dev, m_midi_out_dev);
	descspace.Append(tmp);
	return descspace.Get();
}

const char* CSurf_Faderport::GetConfigString() {
	sprintf(configtmp, "%d %d", m_midi_in_dev, m_midi_out_dev);
	return configtmp;
}

void CSurf_Faderport::ScheduleAction(DWORD time, ScheduleFunc func) {
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

static IReaperControlSurface* createFunc(const char* type_string, const char* configString, int* errStats)
{
	int parms[2];
	parseParms(configString, parms);

	return new CSurf_Faderport(parms[0], parms[1], errStats);
}

static HWND configFunc(const char* type_string, HWND parent, const char* initConfigString)
{
	return CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_SURFACEEDIT_MCU1), parent, dlgProc, (LPARAM)initConfigString);
}

reaper_csurf_reg_t csurf_faderport_reg =
{
  "FADERPORTNATIVE",
  "Presonus Faderport 16/8 Native",
  createFunc,
  configFunc,
};

void CSurf_Faderport::LoadConfig(char* pfilename) {
	start_track = 1;
	start_bus = 0;
	mcp_mode = 0;
	faderport = 8;
	follow = 1;

	std::string path = GetResourcePath();
	std::string filename = pfilename;
	std::string fullpath = path + "\\UserPlugins\\" + filename;

	// Names of the variables in the config file.
	std::vector<std::string> ln = { "faderport","start_track","bus_prefix","audio_prefix","vi_prefix","start_bus","track_fix","mcp_mode", "follow"};

	// Open the config file for reading
	std::ifstream f_in(fullpath);
	if (!f_in)
		cout << "Error reading file !" << endl;
	else
	{
		CFG::ReadFile(f_in, ln, faderport, start_track, bus_prefix, audio_prefix, vi_prefix, start_bus, track_fix, mcp_mode, follow);
		f_in.close();
	}
}
#pragma endregion init