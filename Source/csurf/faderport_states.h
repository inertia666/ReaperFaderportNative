#ifndef _FADERPORTSTATES_H_
#define _FADERPORTSTATES_H_

#include <string>
using namespace std;

class FaderportStates {
private:
	bool is_fp8;
	int m_surface_id;
	int m_bus_view_state;
	int m_vca_view_state;
	int m_all_view_state;
	std::string m_bus_prefix;
	int m_shift_state;
	int m_channel_state;
	int m_bank_state;
	int m_arm_state;
	int m_master_state;
	int m_pan_state;
	int m_link_state;
	int m_track_offset;
	int m_prevnext_offset;
	int m_spread_prevnext_offset;
	int m_surface_size;
	int m_csurf_mcpmode;
	int m_do_prevnext;
	int m_spread_state;

public:
	FaderportStates();

	void SetSpread(int state) { m_spread_state = state; }
	int GetSpread() { return m_spread_state; }
	void ToggleSpread() { m_spread_state = !m_spread_state; }

	void SetSurfaceId(int state) { m_surface_id = state; }
	int GetSurfaceId() { return m_surface_id; }

	void SetBusView(int state) { m_bus_view_state = state; }
	int GetBusView() { return m_bus_view_state; }
	void ToggleBusView() { 
		m_bus_view_state = !m_bus_view_state; 
		m_vca_view_state = !m_bus_view_state;
		m_all_view_state = !m_bus_view_state;
	}

	void SetVCAView(int state) { m_vca_view_state = state; }
	int GetVCAView() { return m_vca_view_state; }
	void ToggleVCAView() { 
		m_vca_view_state = !m_vca_view_state; 
		m_bus_view_state = !m_vca_view_state;
		m_all_view_state = !m_vca_view_state;
	}

	void SetAllView(int state) { m_all_view_state = state; }
	int GetAllView() { return m_all_view_state; }
	void ToggleAllView() {
		m_all_view_state = !m_all_view_state;
		m_bus_view_state = !m_all_view_state;
		m_vca_view_state = !m_all_view_state;
	}

	void SetBusPrefix(std::string state) { m_bus_prefix = state; }
	std::string GetBusPrefix() { return m_bus_prefix; }

	void SetDoPrevNext(int state) { m_do_prevnext = state; }
	int GetDoPrevNext() { return m_do_prevnext; }

	void SetIsFP8(int state) { is_fp8 = state; }
	int GetIsFP8() { return is_fp8; }

	void SetShift(int state) { m_shift_state = state; }
	int GetShift() { return m_shift_state; }
	void ToggleShift() { m_shift_state = !m_shift_state; }

	void SetBank(int state) { m_bank_state = state; }
	int GetBank() { return m_bank_state; }
	void ToggleBank() {
		m_bank_state = !m_bank_state;
		m_channel_state = !m_bank_state;
	}

	void SetArm(int state) { m_arm_state = state; }
	int GetArm() { return m_arm_state; }
	void ToggleArm() { m_arm_state = !m_arm_state; }

	void SetMaster(int state) { m_master_state = state; }
	int GetMaster() { return m_master_state; }
	void ToggleMaster() { m_master_state = !m_master_state; }

	void SetLink(int state) { m_link_state = state; }
	int GetLink() { return m_link_state; }
	void ToggleLink() { m_link_state = !m_link_state; }

	void SetTrackOffset(int state) { m_track_offset = state; }
	int GetTrackOffset() { return m_track_offset; }

	void SetPrevNextOffset(int state) { m_prevnext_offset = state; }
	int GetPrevNextOffset() { return m_prevnext_offset; }

	void SetSpreadPrevNextOffset(int state) { m_prevnext_offset = state; }
	int GetSpreadPrevNextOffset() { return m_prevnext_offset; }

	void SetSurfaceSize(int state) { m_surface_size = state; }
	int GetSurfaceSize() { return m_surface_size; }

	void SetMCPMode(int state) { m_csurf_mcpmode = state; }
	int GetMCPMode() { return m_csurf_mcpmode; }
	void ToggleMCPMode() { m_csurf_mcpmode = !m_csurf_mcpmode; }
};
#endif