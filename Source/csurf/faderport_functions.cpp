#pragma once

#include "faderport_functions.h"
#include "csurf_faderport.h"

FaderportFunctions::FaderportFunctions() : m_states(), m_midiout()
{
}

// return the starting point of the surface
int FaderportFunctions::GetCurrentSurfaceOffset() {
	int trackOffset = m_states->GetTrackOffset();
	int BankOffset = m_states->GetPrevNextOffset();
	return  trackOffset + BankOffset;
}

// Return the correct surface display id from a media track
int FaderportFunctions::GetSurfaceDisplayIdFromReaperMediaTrack(MediaTrack* track) {
	// look up virtual surface for the track

	int id = CSurf_TrackToID(track, m_states->GetMCPMode());

	if (id > 0) {
		//id -= GetReaperTrackId(0);
		return (id == m_states->GetSurfaceSize()) ? -1 : id;
	}

	return id == 0 ? m_states->GetSurfaceSize() : id;
}

// Convert the surface display id to it's corresponding select button
int FaderportFunctions::SurfaceDisplayIdToSelectBtn(int id) {
	return	id == 8 ? id = B_SELECT9 : id += B_SELECT1;
}

// Convert the surface select button to a track id
int FaderportFunctions::SurfaceSelectBtnToReaperTrackId(int midi_code) {
	int id = midi_code - 0x18;

	// Map button 9 correctly
	midi_code == B_SELECT9 ? id = 8 : false;

	return id;
}

int FaderportFunctions::SurfaceMuteBtnToReaperTrackId(int midi_code) {
	int mute_id = midi_code;
	int id = mute_id - B_MUTE1;

	(!m_states->GetIsFP8() && mute_id >= B_MUTE9) ? id = mute_id - 0x70 : false;

	return id;
}

int FaderportFunctions::SurfaceSoloBtnToReaperTrackId(int midi_code) {
	int soloId = midi_code;
	int id = soloId - B_SOLO1; // solo 1 button starts at 0x08

	if (soloId >= B_SOLO9 && soloId <= B_SOLO14) id = soloId - 0x48; // solo 9 button starts at 0x50
	if (soloId == B_SOLO12) id = 11; // solo 12
	if (soloId == B_SOLO15) id = 14; // solo 15
	if (soloId == B_SOLO16) id = 15; // solo 16

	return id;
}

void FaderportFunctions::ClearScribbleText(int id) {
	char buf[7] = { 0, };
	SendTextToScribble(id, 0, 0, buf);
	SendTextToScribble(id, 1, 0, buf);
	//SendTextToScribble(id, 2, 0, buf);
}

// Initialise the scribble display
void FaderportFunctions::ResetScribble(int id) {
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
	poo.evt.midi_message[4] = m_states->GetIsFP8() ? FP8 : FP16;

	poo.evt.midi_message[5] = SCRIBBLE;
	poo.evt.midi_message[6] = 0x00 + id;
	poo.evt.midi_message[7] = SCRIBBLE_DEFAULT_TEXT_METERING;
	poo.evt.midi_message[8] = 0xF7;

	m_midiout->SendMsg(&poo.evt, -1);
}

void FaderportFunctions::SendTextToScribble(int scribbleId, int lineNumber, int alignment, std::string txt) {
	if (m_midiout == NULL) return;

	const char* text = txt.c_str();

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
	poo.evt.midi_message[poo.evt.size++] = m_states->GetIsFP8() ? FP8 : FP16;

	//<SysExHdr> 12, xx, yy, zz, tx,tx,tx,... F7
	poo.evt.midi_message[poo.evt.size++] = SCRIBBLE_SEND_STRING;
	poo.evt.midi_message[poo.evt.size++] = 0x00 + scribbleId;				// xx strip id
	poo.evt.midi_message[poo.evt.size++] = 0x00 + lineNumber;		// yy line number 0-3
	poo.evt.midi_message[poo.evt.size++] = 0x0000000 + alignment;	// zz alignment flag 0000000=centre

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

void FaderportFunctions::SetBtnColour(int btn_id, int rgb_colour) {
	if (m_midiout == NULL) return;

	unsigned int red = (rgb_colour >> 0) & 0xff;
	unsigned int green = (rgb_colour >> 8) & 0xff;
	unsigned int blue = (rgb_colour >> 16) & 0xff;

	m_midiout->Send(RED_CHANNEL, btn_id, 0x00 + int(red > 0x7F ? red * 0.5 : red), -1);
	m_midiout->Send(GREEN_CHANNEL, btn_id, 0x00 + int(green > 0x7F ? green * 0.5 : green), -1);
	m_midiout->Send(BLUE_CHANNEL, btn_id, 0x00 + int(blue > 0x7F ? blue * 0.5 : blue), -1);
}

void FaderportFunctions::ResetValueBar(int id, int mode) {
	// Set valuebar style
	(!m_states->GetIsFP8() && id > 7) ? m_midiout->Send(VALUEBAR, VALUEBAR_MODE_OFFSET_9_16 + (id & 7), mode, -1) : m_midiout->Send(VALUEBAR, VALUEBAR_MODE_OFFSET_1_8 + id, mode, -1);

	// pan centre
	(!m_states->GetIsFP8() && id > 7) ? m_midiout->Send(VALUEBAR, VALUEBAR_VALUE_9_16 + (id & 7), 0x40, -1) : m_midiout->Send(VALUEBAR, VALUEBAR_VALUE_1_8 + id, 0x40, -1);
}

void FaderportFunctions::ResetValueBars() {
	for (int i = 0; i < m_states->GetSurfaceSize(); i++) {
		ResetValueBar(i, MODE_OFF);
	}
}

void FaderportFunctions::ResetFader(int id) {
	m_midiout->Send(FADER + id, 0x40 + 10, STATE_OFF, 0);
}

void FaderportFunctions::ResetFaders() {
	for (int i = 0; i < m_states->GetSurfaceSize(); i++) {
		ResetFader(i);
	}
}

void FaderportFunctions::ResetScribbles() {
	for (int i = 0; i < m_states->GetSurfaceSize(); i++) {
		ResetScribble(i);
	}
}

void FaderportFunctions::ClearScribbleTexts() {
	for (int i = 0; i < m_states->GetSurfaceSize(); i++) {
		ClearScribbleText(i);
	}
}

bool FaderportFunctions::AnySolo() {
	for (TrackIterator ti; !ti.end(); ++ti) {
		MediaTrack* track = *ti;
		int* OriginalState = (int*)GetSetMediaTrackInfo(track, "I_SOLO", NULL);

		if (*OriginalState > 0) return true;
	}

	return false;
}

bool FaderportFunctions::AnyMute() {
	for (TrackIterator ti; !ti.end(); ++ti) {
		MediaTrack* track = *ti;
		int* OriginalState = (int*)GetSetMediaTrackInfo(track, "B_MUTE", NULL);

		if (*OriginalState > 0) return true;
	}

	return false;
}

void FaderportFunctions::ClearSelect(int id) {
	if (!m_states->GetIsFP8() && id == 8) {
		m_midiout->Send(BTN, B_SELECT9, STATE_OFF, -1);
		return;
	}

	(!m_states->GetIsFP8() && id > 7) ? m_midiout->Send(BTN, B_SELECT10 - 1 + (id & 7), STATE_OFF, -1) : m_midiout->Send(BTN, B_SELECT1 + id, STATE_OFF, -1);
}

void FaderportFunctions::ClearSelectBtns() {
	for (int i = 0; i < m_states->GetSurfaceSize(); i++) {
		ClearSelect(i);
	}
}

void FaderportFunctions::ClearShift() {
	//m_pFaderport->m_shift_state = false;
	m_midiout->Send(BTN, B_SHIFTL, STATE_OFF, 0);
	m_midiout->Send(BTN, B_SHIFTR, STATE_OFF, 0);
	SetDefaultButtonColours();
}

void FaderportFunctions::ClearSoloBtn(int id) {
	(id > 7) ? m_midiout->Send(BTN, B_SOLO9 + (id & 7), STATE_OFF, -1) : m_midiout->Send(BTN, B_SOLO1 + id, STATE_OFF, -1);
}

void FaderportFunctions::ClearMuteBtn(int id) {
	(id > 7) ? m_midiout->Send(BTN, B_MUTE9 + (id & 7), STATE_OFF, -1) : m_midiout->Send(BTN, B_MUTE1 + id, STATE_OFF, -1);
}

void FaderportFunctions::ClearMeter(int id) {
	(id > 7) ? m_midiout->Send(PEAK_METER_9_16 + (id & 7), STATE_OFF, 0, -1) : m_midiout->Send(PEAK_METER_1_8 + id, STATE_OFF, 0, -1);
}

void FaderportFunctions::ClearMeters() {
	for (int i = 0; i < m_states->GetSurfaceSize(); i++) {
		ClearMeter(i);
	}
}

void FaderportFunctions::ResetBtns() {
	if (m_midiout == NULL) return;

	m_midiout->Send(BTN, B_TRACK, STATE_ON, -1);
	m_midiout->Send(BTN, B_EDITPLUGINS, STATE_OFF, -1);
	m_midiout->Send(BTN, B_SENDS, STATE_OFF, -1);
	m_midiout->Send(BTN, B_PAN, STATE_OFF, -1);

	m_midiout->Send(BTN, B_AUDIO, STATE_OFF, -1);
	m_midiout->Send(BTN, B_VI, STATE_OFF, -1);
	m_midiout->Send(BTN, B_BUS, STATE_OFF, -1);
	m_midiout->Send(BTN, B_VCA, STATE_OFF, -1);
	m_midiout->Send(BTN, B_ALL, !m_states->GetMCPMode() ? STATE_ON : STATE_OFF, -1);

	m_midiout->Send(BTN, B_CHANNEL, m_states->GetChannel() ? STATE_ON : STATE_OFF, -1);
	m_midiout->Send(BTN, B_BANK, m_states->GetBank() ? STATE_ON : STATE_OFF, -1);

	m_midiout->Send(BTN, B_MUTE_CLEAR, STATE_OFF, -1);
	m_midiout->Send(BTN, B_SOLO_CLEAR, STATE_OFF, -1);

	m_midiout->Send(BTN, B_SHIFTL, STATE_OFF, -1);
	m_midiout->Send(BTN, B_SHIFTR, STATE_OFF, -1);

	m_midiout->Send(BTN, B_AUTO_OFF, STATE_OFF, -1);

	m_midiout->Send(BTN, B_AUTO_LATCH, STATE_OFF, -1);
	m_midiout->Send(BTN, B_AUTO_TRIM, STATE_ON, -1);
	m_midiout->Send(BTN, B_AUTO_TOUCH, STATE_OFF, -1);
	m_midiout->Send(BTN, B_AUTO_WRITE, STATE_OFF, -1);
	m_midiout->Send(BTN, B_AUTO_READ, STATE_OFF, -1);

	m_midiout->Send(BTN, B_PREV, STATE_OFF, -1);
	m_midiout->Send(BTN, B_NEXT, STATE_OFF, -1);

	m_midiout->Send(BTN, B_ZOOM, STATE_OFF, -1);
	m_midiout->Send(BTN, B_SCROLL, STATE_OFF, -1);
	m_midiout->Send(BTN, B_MASTER, STATE_OFF, -1);
	m_midiout->Send(BTN, B_SECTION, STATE_OFF, -1);
	m_midiout->Send(BTN, B_MARKER, STATE_OFF, -1);

	m_midiout->Send(BTN, B_ARM, STATE_OFF, -1);
	m_midiout->Send(BTN, B_BYPASS, STATE_OFF, -1);
	m_midiout->Send(BTN, B_MACRO, STATE_OFF, -1);
	m_midiout->Send(BTN, B_LINK, STATE_OFF, -1);
}

bool FaderportFunctions::isBtn(MIDI_event_t* evt) {
	return ((evt->midi_message[0] & 0xf0) == BTN && evt->midi_message[2] >= 0x40) && !isTouchFader(evt);
}

bool FaderportFunctions::isFader(MIDI_event_t* evt) {
	return (evt->midi_message[0] & 0xf0) == FADER;
}

bool FaderportFunctions::isTouchFader(MIDI_event_t* evt) {
	return (evt->midi_message[0] == 0x90 && evt->midi_message[1] == 0x68 && evt->midi_message[2] == 0x7F);
}

bool FaderportFunctions::isParamFXEncoder(MIDI_event_t* evt) {
	return ((evt->midi_message[0] & 0xf0) == ENCODER && evt->midi_message[1] == PAN_PARAM);
}

bool FaderportFunctions::isSessionNavigationEncoder(MIDI_event_t* evt) {
	return ((evt->midi_message[0] & 0xf0) == ENCODER && evt->midi_message[1] == LARGE_ENCODER);
}

bool FaderportFunctions::isPan(MIDI_event_t* evt) {
	return evt->midi_message[1] >= 0x10 && evt->midi_message[1] < 0x18;
}

void FaderportFunctions::SetDefaultButtonColours() {
	SetBtnColour(B_AUTO_LATCH, 16777471);
	SetBtnColour(B_AUTO_TRIM, 16777471);
	SetBtnColour(B_AUTO_WRITE, 16777471);
	SetBtnColour(B_AUTO_TOUCH, 16810239);
	SetBtnColour(B_AUTO_READ, 16802816);
	SetBtnColour(B_LINK, 33554431);
}