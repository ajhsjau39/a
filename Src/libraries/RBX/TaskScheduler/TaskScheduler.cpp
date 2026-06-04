#include "TaskScheduler.hpp"
#include "../Execution/Execution.hpp"
#include <unistd.h>
#include <string>

uintptr_t RBX::TaskScheduler::GetDatamodel() {
    uintptr_t ve = *reinterpret_cast<uintptr_t*>(RBX::Offsets::VisualEngine::Pointer);
    if (!ve) return 0;
    uintptr_t fake = *reinterpret_cast<uintptr_t*>(ve + RBX::Offsets::VisualEngine::FakeDatamodel);
    if (!fake) return 0;
    return *reinterpret_cast<uintptr_t*>(fake + RBX::Offsets::VisualEngine::Datamodel);
}

uintptr_t RBX::TaskScheduler::GetScriptContext(uintptr_t dm) {
    if (!dm) return 0;
    uintptr_t childrenPtr = *reinterpret_cast<uintptr_t*>(dm + RBX::Offsets::Instance::Children);
    if (!childrenPtr) return 0;
    uintptr_t start = *reinterpret_cast<uintptr_t*>(childrenPtr);
    uintptr_t end   = *reinterpret_cast<uintptr_t*>(childrenPtr + RBX::Offsets::Instance::ChildrenEnd);
    if (!start || !end) return 0;

    for (uintptr_t p = start; p < end; p += 0x10) {
        uintptr_t child = *reinterpret_cast<uintptr_t*>(p);
        if (!child) continue;
        uintptr_t cd = *reinterpret_cast<uintptr_t*>(child + RBX::Offsets::Instance::ClassDescriptor);
        if (!cd) continue;
        const char* name = *reinterpret_cast<const char**>(cd + RBX::Offsets::Instance::ClassName);
        if (name && std::string(name) == "ScriptContext")
            return child;
    }
    return 0;
}

uintptr_t RBX::TaskScheduler::GetPlaceId(uintptr_t dm) {
    if (!dm) return 0;
    return *reinterpret_cast<uintptr_t*>(dm + RBX::Offsets::Datamodel::PlaceId);
}

int RBX::TaskScheduler::GetGameLoadedStatus(uintptr_t dm) {
    if (!dm) return 0;
    return *reinterpret_cast<int*>(dm + RBX::Offsets::Datamodel::GameLoadedStatus);
}

uintptr_t RBX::TaskScheduler::GetGlobalState(uintptr_t sc, uintptr_t* id, uintptr_t* script) {
    if (!sc) return 0;
    return RBX::Functions->GetGlobalState(sc, id, script);
}

void RBX::TaskScheduler::SetIdentity(lua_State* L, int identity, bool) {
    if (!L || !L->userdata) return;
    RobloxExtraSpace* extra = reinterpret_cast<RobloxExtraSpace*>(L->userdata);
    extra->Identity     = identity;
    extra->Capabilities = 0x3FFFFFFFFFFFFFFF;
}

void RBX::TaskScheduler::SendScript(const std::string& script) {
    RBX::Execution::ExecuteScript(RBX::ExecutorState, script);
}