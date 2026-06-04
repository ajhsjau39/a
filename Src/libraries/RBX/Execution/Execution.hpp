#pragma once
#include <string>
#include <lua.h>
#include <lobject.h>
#include <lstate.h>
#include <lapi.h>
#include <lualib.h>
#include <Luau/Compiler.h>
#include <Luau/BytecodeUtils.h>
#include "../RBX.hpp"
#include "../HorizonMac/Src/Offsets.hpp"

namespace RBX::Execution {
    std::string CompileScript(const std::string& script);
    void SetProtoCapabilities(Proto* proto, uintptr_t* caps);
    void ExecuteScript(lua_State* L, const std::string& script);
}