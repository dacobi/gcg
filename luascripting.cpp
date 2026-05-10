#include "luascripting.h"
#include <iostream>
#include <chrono>

LuaScripting* LuaScripting::instance = nullptr;

LuaScripting::LuaScripting(AddBouncerFunc addFunc, DelBouncerFunc delFunc, SetBGFunc bgFunc, SelectFunc selectFunc, SetParamFunc setParamFunc, RandomizeFunc randomizeFunc, SetAudioFunc audioFunc, RecordFunc recordFunc)
    : addBouncerFunc(addFunc), delBouncerFunc(delFunc), setBGFunc(bgFunc), selectFunc(selectFunc), setParamFunc(setParamFunc), randomizeFunc(randomizeFunc), setAudioFunc(audioFunc), recordFunc(recordFunc) {
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
    lua_register(L, "setBG", lua_setBG);
    lua_register(L, "selectPlasma", lua_selectPlasma);
    lua_register(L, "selectFractal", lua_selectFractal);
    lua_register(L, "setPlasmaParam", lua_setPlasmaParam);
    lua_register(L, "setFractalParam", lua_setFractalParam);
    lua_register(L, "randomizePlasmaPalette", lua_randomizePlasmaPalette);
    lua_register(L, "randomizePlasmaXY", lua_randomizePlasmaXY);
    lua_register(L, "randomizeFractalPalette", lua_randomizeFractalPalette);
    lua_register(L, "setAudio", lua_setAudio);
    lua_register(L, "startRecord", lua_startRecord);
    lua_register(L, "stopRecord", lua_stopRecord);
    lua_register(L, "setRecordMax", lua_setRecordMax);
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

int LuaScripting::lua_setBG(lua_State* L) {
    if (lua_isstring(L, 1)) {
        std::string bg = lua_tostring(L, 1);
        if (instance && instance->setBGFunc) {
            instance->setBGFunc(bg);
        }
    }
    return 0;
}

int LuaScripting::lua_selectPlasma(lua_State* L) {
    if (lua_isinteger(L, 1)) {
        int index = (int)lua_tointeger(L, 1);
        if (instance && instance->selectFunc) {
            instance->selectFunc(true, index);
        }
    }
    return 0;
}

int LuaScripting::lua_selectFractal(lua_State* L) {
    if (lua_isinteger(L, 1)) {
        int index = (int)lua_tointeger(L, 1);
        if (instance && instance->selectFunc) {
            instance->selectFunc(false, index);
        }
    }
    return 0;
}

int LuaScripting::lua_setPlasmaParam(lua_State* L) {
    if (lua_isstring(L, 1) && lua_isnumber(L, 2)) {
        std::string name = lua_tostring(L, 1);
        double val = lua_tonumber(L, 2);
        if (instance && instance->setParamFunc) {
            instance->setParamFunc(true, name, val);
        }
    }
    return 0;
}

int LuaScripting::lua_setFractalParam(lua_State* L) {
    if (lua_isstring(L, 1) && lua_isnumber(L, 2)) {
        std::string name = lua_tostring(L, 1);
        double val = lua_tonumber(L, 2);
        if (instance && instance->setParamFunc) {
            instance->setParamFunc(false, name, val);
        }
    }
    return 0;
}

int LuaScripting::lua_randomizePlasmaPalette(lua_State* L) {
    if (instance && instance->randomizeFunc) {
        instance->randomizeFunc(true, false);
    }
    return 0;
}

int LuaScripting::lua_randomizePlasmaXY(lua_State* L) {
    if (instance && instance->randomizeFunc) {
        instance->randomizeFunc(true, true);
    }
    return 0;
}

int LuaScripting::lua_randomizeFractalPalette(lua_State* L) {
    if (instance && instance->randomizeFunc) {
        instance->randomizeFunc(false, false);
    }
    return 0;
}

int LuaScripting::lua_setAudio(lua_State* L) {
    if (lua_isstring(L, 1)) {
        std::string path = lua_tostring(L, 1);
        if (instance && instance->setAudioFunc) {
            instance->setAudioFunc(path);
        }
    }
    return 0;
}

int LuaScripting::lua_startRecord(lua_State* L) {
    if (lua_isstring(L, 1)) {
        std::string path = lua_tostring(L, 1);
        if (instance && instance->recordFunc) {
            instance->recordFunc(0, path, 0);
        }
    }
    return 0;
}

int LuaScripting::lua_stopRecord(lua_State* L) {
    if (instance && instance->recordFunc) {
        instance->recordFunc(1, "", 0);
    }
    return 0;
}

int LuaScripting::lua_setRecordMax(lua_State* L) {
    if (lua_isinteger(L, 1)) {
        int seconds = (int)lua_tointeger(L, 1);
        if (instance && instance->recordFunc) {
            instance->recordFunc(2, "", seconds);
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
