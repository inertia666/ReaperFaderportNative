#ifndef _FADERPORTFUNCTIONS_H_
#define _FADERPORTFUNCTIONS_H_
#include "csurf.h"
#include "faderport_states.h"
#include <string>

class CSurf_Faderport;

class FaderportFunctions {
private:
	FaderportStates* m_states;
	midi_Output* m_midiout;

public:
	FaderportFunctions();

	int GetCurrentSurfaceOffset();
	int GetReaperTrackId(int surface_displayid);
	int SurfaceDisplayIdToSelectBtn(int id);
	int GetSurfaceDisplayIdFromReaperMediaTrack(MediaTrack*);
	int SurfaceSelectBtnToReaperTrackId(int midi_code);
	int SurfaceMuteBtnToReaperTrackId(int midi_code);
	int SurfaceSoloBtnToReaperTrackId(int midi_code);

	// Use a pointer to reference the state object
	void SetStates(FaderportStates* states) { m_states = states; }
	FaderportStates* GetStates() { return m_states; }

	void SetMidiout(midi_Output* midiout) { m_midiout = midiout; }
	midi_Output* GetMidiout() { return m_midiout; }

	void SendTextToScribble(int, int lineNumber, int alignment, std::string txt);

	void SetBtnColour(int btn_id, int rgb_colour);

	void ResetValueBar(int, int);
	void ResetValueBars();
	void ResetFader(int);
	void ResetFaders();
	void ResetScribbles();
	void ResetScribble(int id);

	bool AnySolo();
	bool AnyMute();

	void ClearScribbleText(int);
	void ClearScribbleTexts();
	void ClearSelect(int);
	void ClearSelectBtns();
	void ClearShift();
	void ClearSoloBtn(int);
	void ClearMuteBtn(int);
	void ClearMeter(int);
	void ClearMeters();
	void ResetBtns();

	bool isBtn(MIDI_event_t* evt);
	bool isParamFXEncoder(MIDI_event_t* evt);
	bool isSessionNavigationEncoder(MIDI_event_t* evt);
	bool isFader(MIDI_event_t* evt);
	bool isTouchFader(MIDI_event_t* evt);
	bool isPan(MIDI_event_t* evt);

	void SetDefaultButtonColours();
};

#endif