#pragma once
#include <string>
#include <lua.h>
#include <lstate.h>
#include "../RBX.hpp"
#include "../HorizonMac/Src/Offsets.hpp"

namespace RBX::TaskScheduler {
    uintptr_t GetDatamodel();
    uintptr_t GetScriptContext(uintptr_t Datamodel);
    uintptr_t GetPlaceId(uintptr_t Datamodel);
    int       GetGameLoadedStatus(uintptr_t Datamodel);
    uintptr_t GetGlobalState(uintptr_t ScriptContext, uintptr_t* Identity, uintptr_t* Script);
    uintptr_t GetJobByName(std::string name);
    void      SetIdentity(lua_State* L, int Identity, bool IsInstance);
    void      SendScript(const std::string& script);
}