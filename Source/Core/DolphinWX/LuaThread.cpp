// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <lua.hpp>

#include "Core/Core.h"
#include "DolphinWX/LuaScripting.h"
#include "Core/Movie.h"
#include "DolphinWX/Frame.h"

namespace Lua
{

LuaThread::LuaThread(LuaScriptFrame* p, const wxString& file)
    : m_parent(p), m_file_path(file), wxThread()
{
  // Zero out controller
  m_pad_status.button = 0;
  m_pad_status.stickX = GCPadStatus::MAIN_STICK_CENTER_X;
  m_pad_status.stickY = GCPadStatus::MAIN_STICK_CENTER_Y;
  m_pad_status.triggerLeft = 0;
  m_pad_status.triggerRight = 0;
  m_pad_status.substickX = GCPadStatus::C_STICK_CENTER_X;
  m_pad_status.substickY = GCPadStatus::C_STICK_CENTER_Y;

  // Register GetValues()
  Movie::SetGCInputManip([this](GCPadStatus* status, int number)
  {
    GetValues(status);
  }, Movie::GCManipIndex::LuaGCManip);
}

LuaThread::~LuaThread()
{
	// Nullify GC manipulator function to prevent crash when lua console is closed
	Movie::SetGCInputManip(nullptr, Movie::GCManipIndex::LuaGCManip);
	m_parent->NullifyLuaThread();
}

wxThread::ExitCode LuaThread::Entry()
{
  std::unique_ptr<lua_State, decltype(&lua_close)> state(luaL_newstate(), lua_close);

  // Register
  lua_sethook(state.get(), &HookFunction, LUA_MASKLINE, 0);

  // Make standard libraries available to loaded script
  luaL_openlibs(state.get());

  //Make custom libraries available to loaded script
  luaopen_libs(state.get());

  if (luaL_loadfile(state.get(), m_file_path) != LUA_OK)
  {
    m_parent->Log("Error opening file.\n");

    return reinterpret_cast<wxThread::ExitCode>(-1);
  }

  // Pause emu
  Core::SetState(Core::CORE_PAUSE);

  if (lua_pcall(state.get(), 0, LUA_MULTRET, 0) != LUA_OK)
  {
    m_parent->Log(lua_tostring(state.get(), 1));

    return reinterpret_cast<wxThread::ExitCode>(-1);
  }
  Exit();
  return reinterpret_cast<wxThread::ExitCode>(0);
}

void LuaThread::GetValues(GCPadStatus *PadStatus)
{
  
  if (LuaThread::m_pad_status.stickX != GCPadStatus::MAIN_STICK_CENTER_X)
    PadStatus->stickX = LuaThread::m_pad_status.stickX;

  if (LuaThread::m_pad_status.stickY != GCPadStatus::MAIN_STICK_CENTER_Y)
	  PadStatus->stickY = LuaThread::m_pad_status.stickY;

  if (LuaThread::m_pad_status.triggerLeft != 0)
	  PadStatus->triggerLeft = LuaThread::m_pad_status.triggerLeft;

  if (LuaThread::m_pad_status.triggerRight != 0)
	  PadStatus->triggerRight = LuaThread::m_pad_status.triggerRight;

  if (LuaThread::m_pad_status.substickX != GCPadStatus::C_STICK_CENTER_X)
	  PadStatus->substickX = LuaThread::m_pad_status.substickX;

  if (LuaThread::m_pad_status.substickY != GCPadStatus::C_STICK_CENTER_Y)
	  PadStatus->substickY = LuaThread::m_pad_status.substickY;

  PadStatus->button |= LuaThread::m_pad_status.button;

  //Update internal gamepad representation with the same struct we're sending out
  m_last_pad_status = *PadStatus;
}

void HookFunction(lua_State* L, lua_Debug* ar)
{
  if (LuaScriptFrame::GetCurrentInstance()->GetLuaThread()->m_destruction_flag)
  {
    luaL_error(L, "Script exited.\n");
  }
}

}  // namespace Lua
