#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <queue>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

class LuaScripting {
public:
    using AddBouncerFunc = std::function<void(const std::string&)>;
    using DelBouncerFunc = std::function<void(int)>;
    using SetBGFunc = std::function<void(const std::string&)>;

    LuaScripting(AddBouncerFunc addFunc, DelBouncerFunc delFunc, SetBGFunc setBGFunc);
    ~LuaScripting();

    bool runScript(const std::string& filename);
    void stop();

private:
    static int lua_addBouncer(lua_State* L);
    static int lua_delBouncer(lua_State* L);
    static int lua_setBG(lua_State* L);
    static int lua_delay(lua_State* L);

    void scriptThreadFunc(std::string filename);

    lua_State* L = nullptr;
    std::thread scriptThread;
    std::atomic<bool> running{false};

    AddBouncerFunc addBouncerFunc;
    DelBouncerFunc delBouncerFunc;
    SetBGFunc setBGFunc;

    static LuaScripting* instance;
};
