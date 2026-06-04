#include "Environment.hpp"
#include "../RBX.hpp"
#include "../Execution/Execution.hpp"
#include "../HorizonMac/Src/Offsets.hpp"
#include <lua.h>
#include <lstate.h>
#include <lapi.h>
#include <lobject.h>
#include <lualib.h>
#include <string>
#include <fstream>
#include <filesystem>
#include <vector>
#include <sys/stat.h>

namespace fs = std::filesystem;
static const std::string WORKSPACE = "workspace/";

#define ttisclosure(o) (ttype(o) == LUA_TFUNCTION)

static void EnsureWorkspace() {
    if (!fs::exists(WORKSPACE))
        fs::create_directory(WORKSPACE);
}

// ── Closure registry ──────────────────────────────────────────
static void InitClosureRegistry(lua_State* L) {
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, "__executor_closures");
}

static void TagExecutorClosure(lua_State* L, int idx) {
    lua_getfield(L, LUA_REGISTRYINDEX, "__executor_closures");
    lua_pushvalue(L, idx);
    lua_pushboolean(L, 1);
    lua_settable(L, -3);
    lua_pop(L, 1);
}

static bool IsRegisteredClosure(lua_State* L, int idx) {
    lua_getfield(L, LUA_REGISTRYINDEX, "__executor_closures");
    lua_pushvalue(L, idx);
    lua_gettable(L, -2);
    bool r = lua_toboolean(L, -1);
    lua_pop(L, 2);
    return r;
}

// ── Executor functions ────────────────────────────────────────
static int loadstring_fn(lua_State* L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    size_t len = 0;
    const char* src = lua_tolstring(L, 1, &len);
    const char* chunk = luaL_optstring(L, 2, "Horizon");

    if (len > 0 && src[0] == '\x1b') {
        lua_pushnil(L);
        lua_pushstring(L, "Luau bytecode execution is forbidden");
        return 2;
    }

    std::string bc = RBX::Execution::CompileScript(src);
    if (bc.empty() || bc[0] == '\0') {
        lua_pushnil(L);
        lua_pushstring(L, bc.size() > 1 ? bc.c_str() + 1 : "Syntax error");
        return 2;
    }

    if (luau_load(L, chunk, bc.c_str(), bc.size(), 0) != LUA_OK) {
        lua_pushnil(L);
        lua_pushvalue(L, -2);
        return 2;
    }

    Closure* fn = clvalue(luaA_toobject(L, -1));
    if (fn && !fn->isC && fn->l.p)
        RBX::Execution::SetProtoCapabilities(fn->l.p, &RBX::MaxCapabilities);

    TagExecutorClosure(L, -1);
    return 1;
}

static int identifyexecutor_fn(lua_State* L) {
    lua_pushstring(L, "Horizon");
    lua_pushstring(L, "1.0.0");
    return 2;
}

static int getgenv_fn(lua_State* L) {
    lua_pushvalue(L, LUA_GLOBALSINDEX);
    return 1;
}

static int isrbxactive_fn(lua_State* L) {
    // macOS: no foreground window API — always return true
    lua_pushboolean(L, 1);
    return 1;
}

static int iscclosure_fn(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    Closure* cl = clvalue(luaA_toobject(L, 1));
    lua_pushboolean(L, cl && cl->isC);
    return 1;
}

static int islclosure_fn(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    Closure* cl = clvalue(luaA_toobject(L, 1));
    lua_pushboolean(L, cl && !cl->isC);
    return 1;
}

static int isexecutorclosure_fn(lua_State* L) {
    if (lua_type(L, 1) != LUA_TFUNCTION) { lua_pushboolean(L, false); return 1; }
    Closure* cl = clvalue(luaA_toobject(L, 1));
    if (!cl) { lua_pushboolean(L, false); return 1; }
    if (IsRegisteredClosure(L, 1)) { lua_pushboolean(L, true); return 1; }
    if (!cl->isC && cl->l.p && cl->l.p->userdata == &RBX::MaxCapabilities) {
        lua_pushboolean(L, true); return 1;
    }
    lua_pushboolean(L, false);
    return 1;
}

static int getthreadidentity_fn(lua_State* L) {
    if (!L->userdata) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, reinterpret_cast<RobloxExtraSpace*>(L->userdata)->Identity);
    return 1;
}

static int setthreadidentity_fn(lua_State* L) {
    luaL_checktype(L, 1, LUA_TNUMBER);
    if (!L->userdata) return 0;
    reinterpret_cast<RobloxExtraSpace*>(L->userdata)->Identity = (int)lua_tointeger(L, 1);
    return 0;
}

static int getrawmetatable_fn(lua_State* L) {
    luaL_checkany(L, 1);
    const TValue* obj = luaA_toobject(L, 1);
    LuaTable* mt = nullptr;
    if (obj->tt == LUA_TTABLE)    mt = hvalue(obj)->metatable;
    else if (obj->tt == LUA_TUSERDATA) mt = uvalue(obj)->metatable;
    if (mt) {
        L->top->tt = LUA_TTABLE;
        L->top->value.gc = reinterpret_cast<GCObject*>(mt);
        L->top++;
    } else lua_pushnil(L);
    return 1;
}

static int setrawmetatable_fn(lua_State* L) {
    luaL_checkany(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    const TValue* obj = luaA_toobject(L, 1);
    const TValue* mt  = luaA_toobject(L, 2);
    if (obj->tt == LUA_TTABLE)         hvalue(obj)->metatable = hvalue(mt);
    else if (obj->tt == LUA_TUSERDATA) uvalue(obj)->metatable = hvalue(mt);
    lua_pushvalue(L, 1);
    return 1;
}

static int isreadonly_fn(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushboolean(L, hvalue(luaA_toobject(L, 1))->readonly);
    return 1;
}

static int setreadonly_fn(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TBOOLEAN);
    hvalue(luaA_toobject(L, 1))->readonly = (uint8_t)lua_toboolean(L, 2);
    return 0;
}

static int checkcaller_fn(lua_State* L) {
    if (!L->userdata) { lua_pushboolean(L, false); return 1; }
    lua_pushboolean(L, reinterpret_cast<RobloxExtraSpace*>(L->userdata)->Identity >= 7);
    return 1;
}

static int getnamecallmethod_fn(lua_State* L) {
    if (!L->namecall) { lua_pushnil(L); return 1; }
    lua_pushstring(L, getstr(L->namecall));
    return 1;
}

static int clonefunction_fn(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    const TValue* fv = luaA_toobject(L, 1);
    if (!fv || ttype(fv) != LUA_TFUNCTION) { luaL_error(L, "expected function"); return 0; }
    Closure* orig = clvalue(fv);
    if (orig->isC) { luaL_error(L, "cannot clone C closures"); return 0; }

    lua_pushcclosure(L, [](lua_State*) -> int { return 0; }, "HorizonClone", 0);
    Closure* copy = clvalue(luaA_toobject(L, -1));
    copy->isC = 0;
    copy->env = orig->env;
    copy->nupvalues = orig->nupvalues;
    copy->l.p = orig->l.p;
    for (int i = 0; i < orig->nupvalues; i++)
        copy->l.uprefs[i] = orig->l.uprefs[i];
    if (copy->l.p)
        RBX::Execution::SetProtoCapabilities(copy->l.p, &RBX::MaxCapabilities);
    return 1;
}

// ── Filesystem ────────────────────────────────────────────────
static int writefile_fn(lua_State* L) {
    EnsureWorkspace();
    std::ofstream(WORKSPACE + luaL_checkstring(L, 1)) << luaL_checkstring(L, 2);
    return 0;
}
static int readfile_fn(lua_State* L) {
    EnsureWorkspace();
    std::ifstream f(WORKSPACE + luaL_checkstring(L, 1));
    std::string s((std::istreambuf_iterator<char>(f)), {});
    lua_pushstring(L, s.c_str());
    return 1;
}
static int appendfile_fn(lua_State* L) {
    EnsureWorkspace();
    std::ofstream(WORKSPACE + luaL_checkstring(L, 1), std::ios::app) << luaL_checkstring(L, 2);
    return 0;
}
static int makefolder_fn(lua_State* L) {
    fs::create_directories(WORKSPACE + luaL_checkstring(L, 1)); return 0;
}
static int delfile_fn(lua_State* L) {
    fs::remove(WORKSPACE + luaL_checkstring(L, 1)); return 0;
}
static int delfolder_fn(lua_State* L) {
    fs::remove_all(WORKSPACE + luaL_checkstring(L, 1)); return 0;
}
static int isfile_fn(lua_State* L) {
    lua_pushboolean(L, fs::is_regular_file(WORKSPACE + luaL_checkstring(L, 1))); return 1;
}
static int isfolder_fn(lua_State* L) {
    lua_pushboolean(L, fs::is_directory(WORKSPACE + luaL_checkstring(L, 1))); return 1;
}
static int listfiles_fn(lua_State* L) {
    EnsureWorkspace();
    fs::path target = fs::path(WORKSPACE) / luaL_checkstring(L, 1);
    if (!fs::exists(target) || !fs::is_directory(target)) { lua_pushnil(L); return 1; }
    lua_newtable(L);
    int i = 1;
    for (auto& e : fs::directory_iterator(target)) {
        lua_pushstring(L, fs::relative(e.path(), WORKSPACE).string().c_str());
        lua_rawseti(L, -2, i++);
    }
    return 1;
}

// ── HTTP ──────────────────────────────────────────────────────
static int request_fn(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_getfield(L, 1, "Url");
    if (!lua_isstring(L, -1)) luaL_error(L, "request: Url required");
    std::string url = lua_tostring(L, -1); lua_pop(L, 1);

    lua_getfield(L, 1, "Method");
    std::string method = lua_isstring(L, -1) ? lua_tostring(L, -1) : "GET";
    lua_pop(L, 1);

    lua_getfield(L, 1, "Body");
    std::string body = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
    lua_pop(L, 1);

    // Use curl via popen for simplicity on macOS
    std::string cmd = "curl -s -o /tmp/horizon_response -w \"%{http_code}\" -X " + method;
    if (!body.empty()) cmd += " -d '" + body + "'";
    cmd += " '" + url + "'";

    FILE* pipe = popen(cmd.c_str(), "r");
    std::string statusStr;
    if (pipe) {
        char buf[8] = {};
        fread(buf, 1, 7, pipe);
        statusStr = buf;
        pclose(pipe);
    }

    int statusCode = statusStr.empty() ? 0 : std::stoi(statusStr);

    std::string responseBody;
    std::ifstream resp("/tmp/horizon_response");
    if (resp) responseBody = std::string((std::istreambuf_iterator<char>(resp)), {});

    lua_newtable(L);
    lua_pushinteger(L, statusCode);   lua_setfield(L, -2, "StatusCode");
    lua_pushstring(L, responseBody.c_str()); lua_setfield(L, -2, "Body");
    lua_pushboolean(L, statusCode >= 200 && statusCode < 300); lua_setfield(L, -2, "Success");
    lua_pushstring(L, "OK");          lua_setfield(L, -2, "StatusMessage");
    lua_newtable(L);                  lua_setfield(L, -2, "Headers");
    return 1;
}

// ── Clipboard ─────────────────────────────────────────────────
static int setclipboard_fn(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    std::string cmd = std::string("echo '") + text + "' | pbcopy";
    system(cmd.c_str());
    return 0;
}

// ── Console (stdout on macOS) ─────────────────────────────────
static int rconsoleprint_fn(lua_State* L) {
    printf("%s", luaL_checkstring(L, 1)); fflush(stdout); return 0;
}
static int rconsoleclear_fn(lua_State* L) {
    printf("\033[2J\033[H"); fflush(stdout); return 0;
}
static int rconsolesettitle_fn(lua_State* L) {
    printf("\033]0;%s\007", luaL_checkstring(L, 1)); fflush(stdout); return 0;
}
static int rconsolecreate_fn(lua_State* L) { return 0; }
static int rconsoledestroy_fn(lua_State* L) { return 0; }
static int rconsoleinput_fn(lua_State* L) {
    std::string s; std::getline(std::cin, s);
    lua_pushstring(L, s.c_str()); return 1;
}

// ── Register ──────────────────────────────────────────────────
#define REG(fn, name) \
    lua_pushcfunction(L, fn, name); \
    TagExecutorClosure(L, -1); \
    lua_setglobal(L, name)

void RBX::Environment::Register(lua_State* L) {
    InitClosureRegistry(L);

    REG(loadstring_fn,        "loadstring");
    REG(identifyexecutor_fn,  "identifyexecutor");
    REG(getgenv_fn,           "getgenv");
    REG(isrbxactive_fn,       "isrbxactive");
    REG(isrbxactive_fn,       "iswindowactive");
    REG(isrbxactive_fn,       "isgameactive");
    REG(iscclosure_fn,        "iscclosure");
    REG(islclosure_fn,        "islclosure");
    REG(isexecutorclosure_fn, "isexecutorclosure");
    REG(isexecutorclosure_fn, "checkclosure");
    REG(isexecutorclosure_fn, "isourclosure");
    REG(getthreadidentity_fn, "getthreadidentity");
    REG(getthreadidentity_fn, "getidentity");
    REG(getthreadidentity_fn, "getthreadcontext");
    REG(setthreadidentity_fn, "setthreadidentity");
    REG(setthreadidentity_fn, "setidentity");
    REG(setthreadidentity_fn, "setthreadcontext");
    REG(getrawmetatable_fn,   "getrawmetatable");
    REG(setrawmetatable_fn,   "setrawmetatable");
    REG(isreadonly_fn,        "isreadonly");
    REG(setreadonly_fn,       "setreadonly");
    REG(checkcaller_fn,       "checkcaller");
    REG(getnamecallmethod_fn, "getnamecallmethod");
    REG(clonefunction_fn,     "clonefunction");
    REG(request_fn,           "request");
    REG(request_fn,           "http_request");
    REG(setclipboard_fn,      "setclipboard");
    REG(setclipboard_fn,      "toclipboard");

    // Filesystem
    REG(writefile_fn,  "writefile");
    REG(readfile_fn,   "readfile");
    REG(appendfile_fn, "appendfile");
    REG(makefolder_fn, "makefolder");
    REG(delfile_fn,    "delfile");
    REG(delfolder_fn,  "delfolder");
    REG(isfile_fn,     "isfile");
    REG(isfolder_fn,   "isfolder");
    REG(listfiles_fn,  "listfiles");

    // Console
    REG(rconsoleprint_fn,   "rconsoleprint");
    REG(rconsoleprint_fn,   "consoleprint");
    REG(rconsoleclear_fn,   "rconsoleclear");
    REG(rconsoleclear_fn,   "consoleclear");
    REG(rconsolesettitle_fn,"rconsolesettitle");
    REG(rconsolesettitle_fn,"consolesettitle");
    REG(rconsolecreate_fn,  "rconsolecreate");
    REG(rconsolecreate_fn,  "consolecreate");
    REG(rconsoledestroy_fn, "rconsoledestroy");
    REG(rconsoledestroy_fn, "consoledestroy");
    REG(rconsoleinput_fn,   "rconsoleinput");
    REG(rconsoleinput_fn,   "consoleinput");
}