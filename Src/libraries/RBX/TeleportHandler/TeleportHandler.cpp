#include "TeleportHandler.hpp"
#include "../TaskScheduler/TaskScheduler.hpp"
#include "../Environment/Environment.hpp"
#include "../RBX.hpp"
#include <lua.h>
#include <lstate.h>
#include <lapi.h>
#include <lualib.h>
#include <unistd.h>
#include <pthread.h>

void RBX::TeleportHandler::Reset() {}

void RBX::TeleportHandler::InitializeExecutor(uintptr_t Datamodel) {
    if (!Datamodel) return;
    if (RBX::TaskScheduler::GetPlaceId(Datamodel) == 0) return;

    while (RBX::TaskScheduler::GetGameLoadedStatus(Datamodel) == 15)
        usleep(100000);

    uintptr_t ScriptContext = RBX::TaskScheduler::GetScriptContext(Datamodel);
    if (!ScriptContext) return;

    RBX::Offsets::ScriptContext::CachedScriptContext = ScriptContext;
    RBX::Offsets::ScriptContext::CachedDataModel     = Datamodel;

    uintptr_t Identity = 0, Script = 0;
    uintptr_t MainThread = RBX::TaskScheduler::GetGlobalState(ScriptContext, &Identity, &Script);
    if (!MainThread) return;

    lua_State* RobloxState = reinterpret_cast<lua_State*>(MainThread);
    lua_State* ExecutorState = lua_newthread(RobloxState);

    void* mem = malloc(sizeof(RobloxExtraSpace));
    if (!mem) return;
    RobloxExtraSpace* src = reinterpret_cast<RobloxExtraSpace*>(RobloxState->userdata);
    if (src) new (mem) RobloxExtraSpace(*src);
    else     new (mem) RobloxExtraSpace();
    ExecutorState->userdata = mem;

    RobloxExtraSpace* extra = reinterpret_cast<RobloxExtraSpace*>(ExecutorState->userdata);
    extra->Identity      = 8;
    extra->Capabilities  = 0x3FFFFFFFFFFFFFFF;
    extra->CapabilityCtx = reinterpret_cast<void*>(0x3FFFFFFFFFFFFFFF);

    RBX::ExecutorState = ExecutorState;

    RBX::TaskScheduler::SetIdentity(ExecutorState, 8, false);
    luaL_sandboxthread(ExecutorState);
    RBX::Environment::Register(ExecutorState);
}

void RBX::TeleportHandler::Initialize() {
    pthread_t t;
    pthread_create(&t, nullptr, [](void*) -> void* {
        uintptr_t LastDatamodel = RBX::TaskScheduler::GetDatamodel();
        if (!LastDatamodel) return nullptr;

        TeleportHandler::InitializeExecutor(LastDatamodel);

        while (true) {
            uintptr_t Current = RBX::TaskScheduler::GetDatamodel();
            if (!Current) { usleep(10000); continue; }

            if (Current != LastDatamodel) {
                TeleportHandler::Reset();
                uintptr_t pid = RBX::TaskScheduler::GetPlaceId(Current);
                if (pid > 0) {
                    while (RBX::TaskScheduler::GetGameLoadedStatus(Current) == 15)
                        usleep(100000);
                    TeleportHandler::InitializeExecutor(Current);
                }
                LastDatamodel = Current;
            }
            usleep(10000);
        }
        return nullptr;
    }, nullptr);
    pthread_detach(t);
}