#ifndef CSURF_FADERPORT_H
#define CSURF_FADERPORT_H

#include "csurf.h"
#include "TrackIterator.h"
#include "WDL/ptrlist.h"
#include "converters.h"
#include "faderport_buttons.h"
#include "faderport_codes.h"
#include "dialoghandler.h"
#include "faderport_functions.h"
#include "faderport_states.h"
#include <string>
#include "faderport_virtualsurface.h"

class CSurf_Faderport : public IReaperControlSurface
{
private:
	int m_midi_in_dev, m_midi_out_dev;

	WDL_String descspace;
	char configtmp[1024]{};

	FaderportStates m_surfaceState;
	FaderportFunctions m_Functions;
	VirtualSurface m_vs_spreadview = {};

#define FADER_REPOS_WAIT 250

public:

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

	void ScheduleAction(DWORD time, ScheduleFunc func);

	DWORD m_fader_lastmove;
	char m_fader_touchstate[256];
	unsigned int m_fader_lasttouch[256]; // m_fader_touchstate changes will clear this, moves otherwise set it. if set to -1, then totally disabled

	// State caches
	// Instance variables follow the current surface

	// Surface track states
	unsigned int m_pan_lasttouch[256];
	unsigned int m_vol_lasttouch[256];
	int m_vol_lastpos[16];
	int m_pan_lastpos[16];
	int m_select_lastpos[16];
	int m_arm_lastpos[16];
	int m_valuebarmode_lastpos[16];
	int m_valuebar_lastpos[16];
	int m_mute_lastpos[16];
	int m_solo_lastpos[16];
	int m_surfacedisplayid_removed[16];
	std::string m_tracktitle_last[16];
	int m_trackcolour_lastpos[16];
	double m_mcu_meterpos[16];

	int m_bus_lastpos = 0;
	int m_vca_lastpos =0;
	int m_audio_lastpos = 0;
	int m_vi_lastpos = 0;
	int m_all_lastpos = 0;
	int m_bank_lastpos = 0;
	int m_channel_lastpos = 0;
	int m_cycle_lastpos =0;
	int m_metronome_lastpos =0;
	int m_send_state_lastpos =0;

	SelectedTrack* m_selected_tracks;
	midi_Output* m_midiout;
	midi_Input* m_midiin;
	ScheduledAction* m_schedule;

	// Double-click
	int m_button_last;
	DWORD m_button_last_time;

	DWORD m_mcu_timedisp_lastforce, m_mcu_meter_lastrun;
	int m_mackie_arrow_states;
	unsigned int m_buttonstate_lastrun;
	unsigned int m_frameupd_lastrun;

	//void AddToSurface(MediaTrack* track, int id);
	void ClearCaches();
	void ClearSelectButtonCache();
	void ClearTrackTitleCache();
	void ClearPrevNextLED();

	bool OnFaderMoveEvt(MIDI_event_t*);
	bool OnTransportEvt(MIDI_event_t*);
	bool OnAutoMode(MIDI_event_t* evt);
	void OnMidiEvent(MIDI_event_t*);
	bool OnParamFXEncoder(MIDI_event_t*);
	bool OnSessionNavigationEncoder(MIDI_event_t*);
	bool OnTouch(MIDI_event_t* evt);

	// Button events
	bool isSendsEvt(MIDI_event_t*);
	bool isMuteEvt(MIDI_event_t*);
	bool isSoloEvt(MIDI_event_t*);
	bool isPrevNextEvt(MIDI_event_t*);
	bool isChannelEvt(MIDI_event_t*);
	bool isBankEvt(MIDI_event_t*);
	bool isSoloClearEvt(MIDI_event_t*);
	bool isMuteClearEvt(MIDI_event_t*);
	bool isChannelSelectEvt(MIDI_event_t*);
	bool isAllEvt(MIDI_event_t*);
	bool isArmEvt(MIDI_event_t*);
	bool isMasterEvt(MIDI_event_t*);
	bool isShiftEvt(MIDI_event_t*);
	bool isPanEvt(MIDI_event_t*);
	bool isAudioEvt(MIDI_event_t*);
	bool isBusEvt(MIDI_event_t*);
	bool isVIEvt(MIDI_event_t*);
	bool isVCAEvt(MIDI_event_t*);
	bool isSave(MIDI_event_t*);
	bool isRedo(MIDI_event_t*);
	bool isUndo(MIDI_event_t*);
	bool isUser1(MIDI_event_t*);
	bool isUser2(MIDI_event_t*);
	bool isUser3(MIDI_event_t*);
	bool isLink(MIDI_event_t*);
	bool isMacro(MIDI_event_t*);

	// Buttons
	void SetChannelBankLED(); // radio behaviour with Bank LED
	void SetLinkLED(); // 
	void SetAudioLED(); //
	void SetBusLED(); //  
	void SetVCALED(); //  
	void SetVILED(); 
	void SetAllLED(); 

	void SoloSelectedTrack(int, bool);
	void MuteSelectedTrack(int, bool);
	void SetMacroLED();

	void SelectTrack(int); // Select a track and light led
	void SelectTrackDoubleClick(int); // Select a track and light LED, deselect all other tracks and LEDs

	void SoloClearAll(int); // Clear all solod tracks and LED
	void MuteClearAll(int); // Clear all muted tracks and LED
	void CacheRepeatState(); // toggle repeat in Reaper and LED
	void CacheAudioViewState(); // Audio LED
	void CacheBusViewState(); // Bus LED
	void CacheVCAViewState(); // VCA LED
	void CacheVIViewState(); // VI LED
	void CacheAllViewState(); // All LED
	void CacheBankState(); //Channel Bank LEDs
	void CacheChannelState(); //Channel Bank LEDs
	void SetArmState();
	void ClearArmState();
	void CacheMetronomeState(); // toggle Metronome in Reaper and LED
	void SetMCPTCPView(); // switch between Mixer or TCP view
	void SetRecordArm(MediaTrack* track);
	void SetMasterState();
	void SetShiftState();
	void SetSendsState();
	void Save();
	void Redo();
	void Undo();
	void User1();
	void User2();
	void User3();

	void PrevNext(int); // move surface tracks by channel or bank
	void DoPrevNext(int);
	void ClearSaveLED();
	void ClearUndoLED();
	void ClearRedoLED();

	void SetPanState();
	void SetAudioViewState();
	void SetBusViewState();
	void SetVCAViewState();
	void SetVIViewState();
	void SetAllViewState();

	void SetChannelState();
	void SetBankState();

	// Reaper poll
	void SetSurfaceFader(int surface_displayid, double volume, double pan);
	void SetSurfaceTrackTitle(int surface_displayid, std::string, int trackId);

	void SyncReaperToSurface(DWORD now);
	void SetSelectLED(MediaTrack* track, int track_id, int selected);
	void SetSelectArmedLED(int track_id, int armed);
	void UpdateSurfaceValueBar(int track_id, double pan);
	void SetMuteLED(int track_id, int mute);
	void SetSoloLED(int track_id, int solo);
	void SetSurfaceTrackColour(int surface_displayid, int colour);
	void SetTrackSelection(MediaTrack* track);
	void UpdateSurfaceMeter(int surface_displayid, int v);
	void UpdateAutomationModes();
	void SelectTrack(MediaTrack*);
	void DeselectTrack(MediaTrack*);
	int CalculateMeter(MediaTrack* track, int surface_displayid, int vubottom, double decay);
	int GetCurrentBankNumber();
	int GetBankNumberFromTrackId(int trackId);
	void MoveToBank(int trackId);
	void CleanUpTrackDisplay(int);
	void GetCurrentVirtualSurfaceView();


	// Reaper API events
	void Run() override;

	// Reaper triggered events
	bool GetTouchState(MediaTrack* track, int isPan) override;
	void SetPlayState(bool play, bool pause, bool rec) override;
	void SetAutoMode(int mode) override;
	void OnTrackSelection(MediaTrack* trackid) override;
	void SetTrackListChange() override;

	int CountAudioTracks();
	int CountVITracks();
	int CountBusTracks();
	int CountVCATracks();

	void UpdateVirtualLayoutViews();
	void PrevNextCheck();

	bool isAudioTrack(MediaTrack*);
	bool isBusTrack(MediaTrack*);
	bool isVCATrack(MediaTrack*);
	bool isVITrack(MediaTrack*);

	// Reaper API
	const char* GetTypeString() override;
	const char* GetDescString() override;
	const char* GetConfigString() override;

	CSurf_Faderport(int indev, int outdev, int* errStats);
	~CSurf_Faderport();

	void LoadConfig(char* filename);

	int start_track; // track offset
	int start_bus; // start in bus mode
	int surface_id;
	int mcp_mode;
	int follow;
	std::string audio_prefix;
	std::string bus_prefix;
	std::string midi_prefix;
	std::string vi_prefix;
	int faderport;
	int link; // surface follows Reaper
	int track_fix; // show only the first x number of tracks on the surface
};

#endif
