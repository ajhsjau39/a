#pragma once
#include <stdint.h>
#include <memory>
#include <lua.h>
#include "../Offsets.hpp"

struct RobloxExtraSpace {
    uintptr_t Identity      = 0;
    uintptr_t Capabilities  = 0;
    void*     CapabilityCtx = nullptr;
    void*     Script        = nullptr;
    void*     ScriptCtx     = nullptr;
};

namespace RBX {
    inline lua_State* ExecutorState   = nullptr;
    inline uintptr_t  MaxCapabilities = 0x3FFFFFFFFFFFFFFF;

    class class_functions {
    public:
        using tGetGlobalState = uintptr_t(*)(uintptr_t, uintptr_t*, uintptr_t*);
        tGetGlobalState GetGlobalState =
            reinterpret_cast<tGetGlobalState>(RBX::Offsets::Luau::GetGlobalState);
    };
    inline auto Functions = std::make_unique<class_functions>();
}