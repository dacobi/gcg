#include "luascripting.h"
#include <iostream>
#include <chrono>

LuaScripting* LuaScripting::instance = nullptr;

LuaScripting::LuaScripting(AddBouncerFunc addFunc, DelBouncerFunc delFunc)
    : addBouncerFunc(addFunc), delBouncerFunc(delFunc) {
    instance = this;
}

LuaScripting::~LuaScripting() {
    stop();
    instance = nullptr;
}

bool LuaScripting::runScript(const std::string& filename) {
    if (running) {
        std::cerr << "Script already running" << std::endl;
        return false;
    }
    running = true;
    scriptThread = std::thread(&LuaScripting::scriptThreadFunc, this, filename);
    return true;
}

void LuaScripting::stop() {
    running = false;
    if (scriptThread.joinable()) {
        scriptThread.join();
    }
}

void LuaScripting::scriptThreadFunc(std::string filename) {
    L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "addBouncer", lua_addBouncer);
    lua_register(L, "delBouncer", lua_delBouncer);
    lua_register(L, "delay", lua_delay);

    if (luaL_dofile(L, filename.c_str()) != LUA_OK) {
        std::cerr << "Lua Error: " << lua_tostring(L, -1) << std::endl;
    }

    lua_close(L);
    L = nullptr;
    running = false;
}

int LuaScripting::lua_addBouncer(lua_State* L) {
    if (lua_isstring(L, 1)) {
        std::string syntax = lua_tostring(L, 1);
        if (instance && instance->addBouncerFunc) {
            instance->addBouncerFunc(syntax);
        }
    }
    return 0;
}

int LuaScripting::lua_delBouncer(lua_State* L) {
    if (lua_isinteger(L, 1)) {
        int index = (int)lua_tointeger(L, 1);
        if (instance && instance->delBouncerFunc) {
            instance->delBouncerFunc(index);
        }
    }
    return 0;
}

int LuaScripting::lua_delay(lua_State* L) {
    if (lua_isinteger(L, 1)) {
        int ms = (int)lua_tointeger(L, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
    return 0;
}
