#include "luascripting.h"
#include "input_manager.h"
#include <iostream>
#include <chrono>

LuaScripting* LuaScripting::instance = nullptr;

LuaScripting::LuaScripting(AddBouncerFunc addFunc, DelBouncerFunc delFunc, SetBGFunc bgFunc, SelectFunc selectFunc, SetParamFunc setParamFunc, RandomizeFunc randomizeFunc, SetAudioFunc audioFunc, RecordFunc recordFunc, IsRecordingFunc isRecFunc, SelectUSDFunc selectUSDFunc, SelectGodotFunc selectGodotFunc, SetUSDParamFunc setUSDParamFunc, GodotCmdFunc godotFunc)
    : addBouncerFunc(addFunc), delBouncerFunc(delFunc), setBGFunc(bgFunc), selectFunc(selectFunc), setParamFunc(setParamFunc), randomizeFunc(randomizeFunc), setAudioFunc(audioFunc), recordFunc(recordFunc), isRecFunc(isRecFunc), selectUSDFunc(selectUSDFunc), selectGodotFunc(selectGodotFunc), setUSDParamFunc(setUSDParamFunc), godotCmdFunc(godotFunc) {
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
    lua_register(L, "selectUSD", lua_selectUSD);
    lua_register(L, "selectGodot", lua_selectGodot);
    lua_register(L, "setPlasmaParam", lua_setPlasmaParam);
    lua_register(L, "setFractalParam", lua_setFractalParam);
    lua_register(L, "setUSDParam", lua_setUSDParam);
    lua_register(L, "randomizePlasmaPalette", lua_randomizePlasmaPalette);
    lua_register(L, "randomizePlasmaXY", lua_randomizePlasmaXY);
    lua_register(L, "randomizeFractalPalette", lua_randomizeFractalPalette);
    lua_register(L, "setAudio", lua_setAudio);
    lua_register(L, "startRecord", lua_startRecord);
    lua_register(L, "stopRecord", lua_stopRecord);
    lua_register(L, "setRecordMax", lua_setRecordMax);
    lua_register(L, "delay", lua_delay);

    // Input Framework
    lua_register(L, "ioKBClicked", lua_ioKBClicked);
    lua_register(L, "ioKBDown", lua_ioKBDown);
    lua_register(L, "ioKBUp", lua_ioKBUp);
    lua_register(L, "ioMousePos", lua_ioMousePos);
    lua_register(L, "ioMouseMoved", lua_ioMouseMoved);
    lua_register(L, "ioMouseGetMotion", lua_ioMouseGetMotion);
    lua_register(L, "ioMouseBTNClicked", lua_ioMouseBTNClicked);
    lua_register(L, "ioMouseBTNDown", lua_ioMouseBTNDown);
    lua_register(L, "ioMouseBTNUp", lua_ioMouseBTNUp);

    // Godot Manipulation
    lua_register(L, "godotSelectRoot", lua_godotSelectRoot);
    lua_register(L, "godotSelectNode", lua_godotSelectNode);
    lua_register(L, "godotSearchNode", lua_godotSearchNode);
    lua_register(L, "godotGetNodeType", lua_godotGetNodeType);
    lua_register(L, "godotGetName", lua_godotGetName);
    lua_register(L, "godotRenameNode", lua_godotRenameNode);
    lua_register(L, "godotSetCamera", lua_godotSetCamera);
    lua_register(L, "godotGetPos", lua_godotGetPos);
    lua_register(L, "godotSetPos", lua_godotSetPos);
    lua_register(L, "godotMoveX", lua_godotMoveX);
    lua_register(L, "godotMoveY", lua_godotMoveY);
    lua_register(L, "godotMoveZ", lua_godotMoveZ);
    lua_register(L, "godotCreateNode", lua_godotCreateNode);
    lua_register(L, "godotLoadNode", lua_godotLoadNode);
    lua_register(L, "godotDeleteNode", lua_godotDeleteNode);

    // Set a hook to abort execution if stop() is called
    lua_sethook(L, lua_hook, LUA_MASKCOUNT, 100);

    if (luaL_dofile(L, filename.c_str()) != LUA_OK) {
        std::string err = lua_tostring(L, -1);
        if (err != "Script terminated") {
            std::cerr << "Lua Error: " << err << std::endl;
        }
    }

    lua_close(L);
    L = nullptr;
    running = false;
}

void LuaScripting::lua_hook(lua_State* L, lua_Debug* ar) {
    if (instance && !instance->running) {
        luaL_error(L, "Script aborted");
    }
}

int LuaScripting::lua_ioKBClicked(lua_State* L) {
    if (lua_isstring(L, 1)) {
        std::string key = lua_tostring(L, 1);
        SDL_Keycode kc = InputManager::stringToKeycode(key);
        lua_pushboolean(L, InputManager::getInstance().lua_isKeyHit(kc));
        return 1;
    }
    return 0;
}

int LuaScripting::lua_ioKBDown(lua_State* L) {
    if (lua_isstring(L, 1)) {
        std::string key = lua_tostring(L, 1);
        SDL_Keycode kc = InputManager::stringToKeycode(key);
        lua_pushboolean(L, InputManager::getInstance().lua_isKeyDown(kc));
        return 1;
    }
    return 0;
}

int LuaScripting::lua_ioKBUp(lua_State* L) {
    if (lua_isstring(L, 1)) {
        std::string key = lua_tostring(L, 1);
        SDL_Keycode kc = InputManager::stringToKeycode(key);
        lua_pushboolean(L, InputManager::getInstance().lua_isKeyUp(kc));
        return 1;
    }
    return 0;
}

int LuaScripting::lua_ioMousePos(lua_State* L) {
    int x, y;
    InputManager::getInstance().lua_getMousePos(x, y);
    lua_pushinteger(L, x);
    lua_pushinteger(L, y);
    return 2;
}

int LuaScripting::lua_ioMouseMoved(lua_State* L) {
    lua_pushboolean(L, InputManager::getInstance().lua_hasMouseMoved());
    return 1;
}

int LuaScripting::lua_ioMouseGetMotion(lua_State* L) {
    int rx, ry;
    InputManager::getInstance().lua_getMouseMotion(rx, ry);
    lua_pushinteger(L, rx);
    lua_pushinteger(L, ry);
    return 2;
}

int LuaScripting::lua_ioMouseBTNClicked(lua_State* L) {
    if (lua_isinteger(L, 1)) {
        int btn = (int)lua_tointeger(L, 1);
        lua_pushboolean(L, InputManager::getInstance().lua_isMouseBtnHit(btn));
        return 1;
    }
    return 0;
}

int LuaScripting::lua_ioMouseBTNDown(lua_State* L) {
    if (lua_isinteger(L, 1)) {
        int btn = (int)lua_tointeger(L, 1);
        lua_pushboolean(L, InputManager::getInstance().lua_isMouseBtnDown(btn));
        return 1;
    }
    return 0;
}

int LuaScripting::lua_ioMouseBTNUp(lua_State* L) {
    if (lua_isinteger(L, 1)) {
        int btn = (int)lua_tointeger(L, 1);
        lua_pushboolean(L, InputManager::getInstance().lua_isMouseBtnUp(btn));
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotSelectRoot(lua_State* L) {
    if (instance && instance->godotCmdFunc) {
        LuaSyncData sd;
        float fargs[3] = {0,0,0};
        instance->godotCmdFunc(GCMD_SELECT_ROOT, "", fargs, &sd);
        std::unique_lock<std::mutex> lock(sd.mtx);
        while (!sd.done && instance && instance->running) {
            sd.cv.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
    return 0;
}

int LuaScripting::lua_godotSelectNode(lua_State* L) {
    if (lua_isstring(L, 1) && instance && instance->godotCmdFunc) {
        LuaSyncData sd;
        float fargs[3] = {0,0,0};
        instance->godotCmdFunc(GCMD_SELECT_NODE, lua_tostring(L, 1), fargs, &sd);
        std::unique_lock<std::mutex> lock(sd.mtx);
        while (!sd.done && instance && instance->running) {
            sd.cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_pushboolean(L, sd.b_res);
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotSearchNode(lua_State* L) {
    if (lua_isstring(L, 1) && instance && instance->godotCmdFunc) {
        LuaSyncData sd;
        float fargs[3] = {0,0,0};
        instance->godotCmdFunc(GCMD_SEARCH_NODE, lua_tostring(L, 1), fargs, &sd);
        std::unique_lock<std::mutex> lock(sd.mtx);
        while (!sd.done && instance && instance->running) {
            sd.cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_pushboolean(L, sd.b_res);
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotGetNodeType(lua_State* L) {
    if (instance && instance->godotCmdFunc) {
        LuaSyncData sd;
        float fargs[3] = {0,0,0};
        instance->godotCmdFunc(GCMD_GET_NODE_TYPE, "", fargs, &sd);
        std::unique_lock<std::mutex> lock(sd.mtx);
        while (!sd.done && instance && instance->running) {
            sd.cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_pushstring(L, sd.s_res.c_str());
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotGetName(lua_State* L) {
    if (instance && instance->godotCmdFunc) {
        LuaSyncData sd;
        float fargs[3] = {0,0,0};
        instance->godotCmdFunc(GCMD_GET_NAME, "", fargs, &sd);
        std::unique_lock<std::mutex> lock(sd.mtx);
        while (!sd.done && instance && instance->running) {
            sd.cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_pushstring(L, sd.s_res.c_str());
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotRenameNode(lua_State* L) {
    if (lua_isstring(L, 1) && instance && instance->godotCmdFunc) {
        LuaSyncData sd;
        float fargs[3] = {0,0,0};
        instance->godotCmdFunc(GCMD_RENAME_NODE, lua_tostring(L, 1), fargs, &sd);
        std::unique_lock<std::mutex> lock(sd.mtx);
        while (!sd.done && instance && instance->running) {
            sd.cv.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
    return 0;
}

int LuaScripting::lua_godotSetCamera(lua_State* L) {
    if (instance && instance->godotCmdFunc) {
        LuaSyncData sd;
        float fargs[3] = {0,0,0};
        instance->godotCmdFunc(GCMD_SET_CAMERA, "", fargs, &sd);
        std::unique_lock<std::mutex> lock(sd.mtx);
        while (!sd.done && instance && instance->running) {
            sd.cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_pushboolean(L, sd.b_res);
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotGetPos(lua_State* L) {
    if (instance && instance->godotCmdFunc) {
        LuaSyncData sd;
        float fargs[3] = {0,0,0};
        instance->godotCmdFunc(GCMD_GET_POS, "", fargs, &sd);
        std::unique_lock<std::mutex> lock(sd.mtx);
        while (!sd.done && instance && instance->running) {
            sd.cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_pushnumber(L, sd.f_res[0]);
        lua_pushnumber(L, sd.f_res[1]);
        lua_pushnumber(L, sd.f_res[2]);
        return 3;
    }
    return 0;
}

int LuaScripting::lua_godotSetPos(lua_State* L) {
    if (lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3) && instance && instance->godotCmdFunc) {
        LuaSyncData sd;
        float fargs[3] = {(float)lua_tonumber(L, 1), (float)lua_tonumber(L, 2), (float)lua_tonumber(L, 3)};
        instance->godotCmdFunc(GCMD_SET_POS, "", fargs, &sd);
        std::unique_lock<std::mutex> lock(sd.mtx);
        while (!sd.done && instance && instance->running) {
            sd.cv.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
    return 0;
}

int LuaScripting::lua_godotMoveX(lua_State* L) {
    if (lua_isnumber(L, 1) && instance && instance->godotCmdFunc) {
        LuaSyncData sd;
        float fargs[3] = {(float)lua_tonumber(L, 1), 0, 0};
        instance->godotCmdFunc(GCMD_MOVE_X, "", fargs, &sd);
        std::unique_lock<std::mutex> lock(sd.mtx);
        while (!sd.done && instance && instance->running) {
            sd.cv.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
    return 0;
}

int LuaScripting::lua_godotMoveY(lua_State* L) {
    if (lua_isnumber(L, 1) && instance && instance->godotCmdFunc) {
        LuaSyncData sd;
        float fargs[3] = {0, (float)lua_tonumber(L, 1), 0};
        instance->godotCmdFunc(GCMD_MOVE_Y, "", fargs, &sd);
        std::unique_lock<std::mutex> lock(sd.mtx);
        while (!sd.done && instance && instance->running) {
            sd.cv.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
    return 0;
}

int LuaScripting::lua_godotMoveZ(lua_State* L) {
    if (lua_isnumber(L, 1) && instance && instance->godotCmdFunc) {
        LuaSyncData sd;
        float fargs[3] = {0, 0, (float)lua_tonumber(L, 1)};
        instance->godotCmdFunc(GCMD_MOVE_Z, "", fargs, &sd);
        std::unique_lock<std::mutex> lock(sd.mtx);
        while (!sd.done && instance && instance->running) {
            sd.cv.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
    return 0;
}

int LuaScripting::lua_godotCreateNode(lua_State* L) {
    if (lua_isstring(L, 1) && instance && instance->godotCmdFunc) {
        LuaSyncData sd;
        float fargs[3] = {0,0,0};
        instance->godotCmdFunc(GCMD_CREATE_NODE, lua_tostring(L, 1), fargs, &sd);
        std::unique_lock<std::mutex> lock(sd.mtx);
        while (!sd.done && instance && instance->running) {
            sd.cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_pushboolean(L, sd.b_res);
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotLoadNode(lua_State* L) {
    if (lua_isstring(L, 1) && instance && instance->godotCmdFunc) {
        LuaSyncData sd;
        float fargs[3] = {0,0,0};
        instance->godotCmdFunc(GCMD_LOAD_NODE, lua_tostring(L, 1), fargs, &sd);
        std::unique_lock<std::mutex> lock(sd.mtx);
        while (!sd.done && instance && instance->running) {
            sd.cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_pushboolean(L, sd.b_res);
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotDeleteNode(lua_State* L) {
    if (instance && instance->godotCmdFunc) {
        LuaSyncData sd;
        float fargs[3] = {0,0,0};
        instance->godotCmdFunc(GCMD_DELETE_NODE, "", fargs, &sd);
        std::unique_lock<std::mutex> lock(sd.mtx);
        while (!sd.done && instance && instance->running) {
            sd.cv.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
    return 0;
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

int LuaScripting::lua_selectUSD(lua_State* L) {
    if (lua_isinteger(L, 1)) {
        int index = (int)lua_tointeger(L, 1);
        if (instance && instance->selectUSDFunc) {
            instance->selectUSDFunc(index);
        }
    }
    return 0;
}

int LuaScripting::lua_selectGodot(lua_State* L) {
    if (lua_isinteger(L, 1)) {
        int index = (int)lua_tointeger(L, 1);
        if (instance && instance->selectGodotFunc) {
            instance->selectGodotFunc(index);
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

int LuaScripting::lua_setUSDParam(lua_State* L) {
    if (lua_isstring(L, 1) && lua_isnumber(L, 2)) {
        std::string name = lua_tostring(L, 1);
        double val = lua_tonumber(L, 2);
        if (instance && instance->setUSDParamFunc) {
            instance->setUSDParamFunc(name, val);
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
    bool wait = false;
    if (lua_isboolean(L, 1)) wait = lua_toboolean(L, 1);
    else if (lua_isinteger(L, 1)) wait = (lua_tointeger(L, 1) != 0);

    if (wait && instance && instance->isRecFunc) {
        // Loop while recording is active
        while (instance->running && instance->isRecFunc()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } else if (instance && instance->recordFunc) {
        instance->recordFunc(1, "", 0); // Immediate stop
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
        int remaining = ms;
        while (remaining > 0 && instance && instance->running) {
            int chunk = std::min(remaining, 100);
            std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
            remaining -= chunk;
        }
    }
    return 0;
}
