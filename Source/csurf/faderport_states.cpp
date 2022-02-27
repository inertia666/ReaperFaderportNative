#pragma once
#include "faderport_states.h";

FaderportStates::FaderportStates() :
	is_fp8(false),
	m_surface_id(0),
	m_shift_state(0),
	m_channel_state(0),
	m_bank_state(0),
	m_arm_state(0),
	m_master_state(0),
	m_pan_state(0),
	m_link_state(0),
	m_track_offset(0),
	m_prevnext_offset(0),
	m_surface_size(0),
	m_csurf_mcpmode(0),
	m_bus_view_state(0),
	m_do_prevnext(0),
	m_spread_state(0),
	m_spread_prevnext_offset(0)
{
}