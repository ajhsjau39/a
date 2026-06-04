#include "Execution.hpp"

namespace {
    class bytecodeEncoder : public Luau::BytecodeEncoder {
        void encode(uint32_t* data, size_t count) override {
            for (size_t i = 0; i < count;) {
                uint8_t op = LUAU_INSN_OP(data[i]);
                auto opLength = Luau::getOpLength((LuauOpcode)op);
                auto* table = reinterpret_cast<uint8_t*>(
                    RBX::Offsets::Luau::OpCodeLookupTable);
                uint8_t newOp = table[op * 227];
                data[i] = newOp | (data[i] & ~0xff);
                i += opLength;
            }
        }
    };
}

std::string RBX::Execution::CompileScript(const std::string& script) {
    bytecodeEncoder encoder;
    const char* mutableGlobals[] = {
        "Game","Workspace","game","plugin","script",
        "shared","workspace","_G","_ENV", nullptr
    };
    Luau::CompileOptions options{};
    options.debugLevel        = 1;
    options.optimizationLevel = 1;
    options.mutableGlobals    = mutableGlobals;
    options.vectorLib  = "Vector3";
    options.vectorCtor = "new";
    options.vectorType = "Vector3";
    return Luau::compile(script, options, {}, &encoder);
}

void RBX::Execution::SetProtoCapabilities(Proto* proto, uintptr_t* caps) {
    if (!proto) return;
    proto->userdata = caps;
    for (int i = 0; i < proto->sizep; i++)
        SetProtoCapabilities(proto->p[i], caps);
}

void RBX::Execution::ExecuteScript(lua_State* L, const std::string& script) {
    if (!L || script.empty()) return;
    int top = lua_gettop(L);

    lua_State* thread = lua_newthread(L);
    if (!thread) { lua_settop(L, top); return; }

    // Copy extra space from parent
    if (!thread->userdata && L->userdata) {
        void* mem = malloc(sizeof(RobloxExtraSpace));
        if (mem) {
            new (mem) RobloxExtraSpace(
                *reinterpret_cast<RobloxExtraSpace*>(L->userdata));
            thread->userdata = mem;
        }
    }

    // Elevate capabilities
    if (thread->userdata) {
        auto* extra = reinterpret_cast<RobloxExtraSpace*>(thread->userdata);
        extra->Identity     = 8;
        extra->Capabilities = 0x3FFFFFFFFFFFFFFF;
    }

    auto bytecode = CompileScript(script);
    if (bytecode.empty() || bytecode[0] == '\0') {
        lua_settop(L, top);
        return;
    }

    if (luau_load(thread, "Horizon", bytecode.c_str(), bytecode.size(), 0) != LUA_OK) {
        lua_settop(L, top);
        return;
    }

    Closure* cl = clvalue(luaA_toobject(thread, -1));
    if (cl && !cl->isC && cl->l.p)
        SetProtoCapabilities(cl->l.p, &RBX::MaxCapabilities);

    lua_getglobal(L, "task");
    lua_getfield(L, -1, "defer");
    lua_remove(L, -2);
    lua_xmove(thread, L, 1);

    if (lua_pcall(L, 1, 0, 0) != LUA_OK)
        lua_pop(L, 1);

    lua_settop(L, top);
}