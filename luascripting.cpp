#include "luascripting.h"
#include "input_manager.h"
#include <iostream>
#include <chrono>

LuaScripting* LuaScripting::instance = nullptr;

LuaScripting::LuaScripting(AddBouncerFunc addFunc, DelBouncerFunc delFunc, SetBGFunc bgFunc, SelectFunc selectFunc, SetParamFunc setParamFunc, RandomizeFunc randomizeFunc, SetAudioFunc audioFunc, RecordFunc recordFunc, IsRecordingFunc isRecFunc, SelectUSDFunc selectUSDFunc, SelectGodotFunc selectGodotFunc, SetUSDParamFunc setUSDParamFunc, GodotCmdFunc godotFunc, QuitFunc quitFunc, SetImGuiVisibleFunc setImGuiVisibleFunc)
    : addBouncerFunc(addFunc), delBouncerFunc(delFunc), setBGFunc(bgFunc), selectFunc(selectFunc), setParamFunc(setParamFunc), randomizeFunc(randomizeFunc), setAudioFunc(audioFunc), recordFunc(recordFunc), isRecFunc(isRecFunc), selectUSDFunc(selectUSDFunc), selectGodotFunc(selectGodotFunc), setUSDParamFunc(setUSDParamFunc), godotCmdFunc(godotFunc), quitFunc(quitFunc), setImGuiVisibleFunc(setImGuiVisibleFunc) {
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

    registerFunctions(L);

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

void LuaScripting::runOneShotScript(const std::string& filename) {
    std::thread([this, filename]() {
        lua_State* L_one = luaL_newstate();
        luaL_openlibs(L_one);
        registerFunctions(L_one);
        if (luaL_dofile(L_one, filename.c_str()) != LUA_OK) {
            std::string err = lua_tostring(L_one, -1);
            std::cerr << "Lua One-Shot Error: " << err << std::endl;
        }
        lua_close(L_one);
    }).detach();
}

void LuaScripting::registerFunctions(lua_State* L_reg) {
    lua_register(L_reg, "addBouncer", lua_addBouncer);
    lua_register(L_reg, "delBouncer", lua_delBouncer);
    lua_register(L_reg, "setBG", lua_setBG);
    lua_register(L_reg, "selectPlasma", lua_selectPlasma);
    lua_register(L_reg, "selectFractal", lua_selectFractal);
    lua_register(L_reg, "selectUSD", lua_selectUSD);
    lua_register(L_reg, "selectGodot", lua_selectGodot);
    lua_register(L_reg, "setPlasmaParam", lua_setPlasmaParam);
    lua_register(L_reg, "setFractalParam", lua_setFractalParam);
    lua_register(L_reg, "setUSDParam", lua_setUSDParam);
    lua_register(L_reg, "randomizePlasmaPalette", lua_randomizePlasmaPalette);
    lua_register(L_reg, "randomizePlasmaXY", lua_randomizePlasmaXY);
    lua_register(L_reg, "randomizeFractalPalette", lua_randomizeFractalPalette);
    lua_register(L_reg, "setAudio", lua_setAudio);
    lua_register(L_reg, "startRecord", lua_startRecord);
    lua_register(L_reg, "stopRecord", lua_stopRecord);
    lua_register(L_reg, "setRecordMax", lua_setRecordMax);
    lua_register(L_reg, "delay", lua_delay);
    lua_register(L_reg, "appQuit", lua_appQuit);
    lua_register(L_reg, "imGuiHide", lua_imGuiHide);
    lua_register(L_reg, "imGuiShow", lua_imGuiShow);

    // Input Framework
    lua_register(L_reg, "ioKBClicked", lua_ioKBClicked);
    lua_register(L_reg, "ioKBDown", lua_ioKBDown);
    lua_register(L_reg, "ioKBUp", lua_ioKBUp);
    lua_register(L_reg, "ioMousePos", lua_ioMousePos);
    lua_register(L_reg, "ioMouseMoved", lua_ioMouseMoved);
    lua_register(L_reg, "ioMouseGetMotion", lua_ioMouseGetMotion);
    lua_register(L_reg, "ioMouseBTNClicked", lua_ioMouseBTNClicked);
    lua_register(L_reg, "ioMouseBTNDown", lua_ioMouseBTNDown);
    lua_register(L_reg, "ioMouseBTNUp", lua_ioMouseBTNUp);

    // Godot Manipulation
    lua_register(L_reg, "godotSelectRoot", lua_godotSelectRoot);
    lua_register(L_reg, "godotSelectNode", lua_godotSelectNode);
    lua_register(L_reg, "godotSearchNode", lua_godotSearchNode);
    lua_register(L_reg, "godotGetNodeType", lua_godotGetNodeType);
    lua_register(L_reg, "godotGetName", lua_godotGetName);
    lua_register(L_reg, "godotRenameNode", lua_godotRenameNode);
    lua_register(L_reg, "godotSetCamera", lua_godotSetCamera);
    lua_register(L_reg, "godotGetPos", lua_godotGetPos);
    lua_register(L_reg, "godotSetPos", lua_godotSetPos);
    lua_register(L_reg, "godotMoveX", lua_godotMoveX);
    lua_register(L_reg, "godotMoveY", lua_godotMoveY);
    lua_register(L_reg, "godotMoveZ", lua_godotMoveZ);
    lua_register(L_reg, "godotMoveAndCollide", lua_godotMoveAndCollide);
    lua_register(L_reg, "godotGetOverlappingAreas", lua_godotGetOverlappingAreas);
    lua_register(L_reg, "godotCreateNode", lua_godotCreateNode);
    lua_register(L_reg, "godotLoadNode", lua_godotLoadNode);
    lua_register(L_reg, "godotDeleteNode", lua_godotDeleteNode);
    lua_register(L_reg, "godotAttachScript", lua_godotAttachScript);
    lua_register(L_reg, "godotSetProperty", lua_godotSetProperty);
    lua_register(L_reg, "godotGetProperty", lua_godotGetProperty);
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

int LuaScripting::lua_godotMoveAndCollide(lua_State* L) {
    if (lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3) && instance && instance->godotCmdFunc) {
        LuaSyncData sd;
        float fargs[3] = {(float)lua_tonumber(L, 1), (float)lua_tonumber(L, 2), (float)lua_tonumber(L, 3)};
        instance->godotCmdFunc(GCMD_MOVE_AND_COLLIDE, "", fargs, &sd);
        std::unique_lock<std::mutex> lock(sd.mtx);
        while (!sd.done && instance && instance->running) {
            sd.cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_pushboolean(L, sd.b_res);
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotGetOverlappingAreas(lua_State* L) {
    if (instance && instance->godotCmdFunc) {
        LuaSyncData sd;
        float fargs[3] = {0,0,0};
        instance->godotCmdFunc(GCMD_GET_OVERLAPPING_AREAS, "", fargs, &sd);
        std::unique_lock<std::mutex> lock(sd.mtx);
        while (!sd.done && instance && instance->running) {
            sd.cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_newtable(L);
        for (size_t i = 0; i < sd.vs_res.size(); ++i) {
            lua_pushinteger(L, i + 1);
            lua_pushstring(L, sd.vs_res[i].c_str());
            lua_settable(L, -3);
        }
        return 1;
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
        int use_pos = 0;
        if (lua_gettop(L) >= 4 && lua_isnumber(L, 2) && lua_isnumber(L, 3) && lua_isnumber(L, 4)) {
            fargs[0] = (float)lua_tonumber(L, 2);
            fargs[1] = (float)lua_tonumber(L, 3);
            fargs[2] = (float)lua_tonumber(L, 4);
            use_pos = 1;
        }
        
        // Repurpose GCMD_LOAD_NODE to use fargs and an index flag
        // In the receiver, we need to pass these to GodotRenderer
        std::string path = lua_tostring(L, 1);
        
        // We'll use a hack to pass use_pos through the existing GodotCmdFunc
        // We can't easily change the signature, but we can use the sync_data pointer
        // or just use fargs[1] as we did for properties.
        // Actually, we'll use a custom internal mechanism or just update GodotCmdFunc.
        
        instance->godotCmdFunc(GCMD_LOAD_NODE, path, fargs, &sd);
        // Wait, fargs only has 3 slots. I need a way to pass use_pos.
        // Let's use a bitmask or a 4th value?
        // Let's check GodotCmdFunc in luascripting.h
        // using GodotCmdFunc = std::function<void(GodotCmd cmd, const std::string& str_arg, float f_args[3], LuaSyncData* sync_data)>;
        
        // I'll use f_args[0..2] for pos, and f_args[0] = special value if no pos? 
        // No, let's use the 'done' field in SyncData temporarily or just pass it in str_arg?
        // Best way: use 'b_res' in SyncData as an *input* flag before calling, 
        // or just assume if top >= 4 then use_pos is true.
        
        // Actually, I can use sd.b_res as an INPUT flag since it's just a struct.
        sd.b_res = (use_pos == 1); 

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

int LuaScripting::lua_godotAttachScript(lua_State* L) {
    if (lua_isstring(L, 1) && instance && instance->godotCmdFunc) {
        LuaSyncData sd;
        float fargs[3] = {0,0,0};
        instance->godotCmdFunc(GCMD_ATTACH_SCRIPT, lua_tostring(L, 1), fargs, &sd);
        std::unique_lock<std::mutex> lock(sd.mtx);
        while (!sd.done && instance && instance->running) {
            sd.cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_pushboolean(L, sd.b_res);
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotSetProperty(lua_State* L) {
    if (lua_isstring(L, 1) && instance && instance->godotCmdFunc) {
        std::string name = lua_tostring(L, 1);
        LuaSyncData sd;
        float fargs[3] = {0,0,0};
        if (lua_isnumber(L, 2)) {
            fargs[0] = (float)lua_tonumber(L, 2);
            instance->godotCmdFunc(GCMD_SET_PROPERTY, name, fargs, &sd); // fargs[0] signals number
        } else if (lua_isstring(L, 2)) {
            // We use a hack: store the value in str_arg with a separator, or just use fargs[1]=1 for string?
            // Actually, let's just use the current command structure and add a value type.
            // For now, I'll pass the value in fargs[0] and use fargs[1] as a type flag.
            // fargs[1] = 0: number, 1: string
            fargs[0] = 0; // Not used for string
            fargs[1] = 1; // Type = string
            std::string combined = name + "|" + lua_tostring(L, 2);
            instance->godotCmdFunc(GCMD_SET_PROPERTY, combined, fargs, &sd);
        } else {
            return 0;
        }
        std::unique_lock<std::mutex> lock(sd.mtx);
        while (!sd.done && instance && instance->running) {
            sd.cv.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
    return 0;
}

int LuaScripting::lua_godotGetProperty(lua_State* L) {
    if (lua_isstring(L, 1) && instance && instance->godotCmdFunc) {
        LuaSyncData sd;
        float fargs[3] = {0,0,0};
        instance->godotCmdFunc(GCMD_GET_PROPERTY, lua_tostring(L, 1), fargs, &sd);
        std::unique_lock<std::mutex> lock(sd.mtx);
        while (!sd.done && instance && instance->running) {
            sd.cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        if (sd.b_res) { // Use b_res to signal if it was a number or string?
            lua_pushnumber(L, sd.d_res);
        } else {
            lua_pushstring(L, sd.s_res.c_str());
        }
        return 1;
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

int LuaScripting::lua_appQuit(lua_State* L) {
    if (instance && instance->quitFunc) {
        instance->quitFunc();
    }
    return 0;
}

int LuaScripting::lua_imGuiHide(lua_State* L) {
    if (instance && instance->setImGuiVisibleFunc) {
        instance->setImGuiVisibleFunc(false);
    }
    return 0;
}

int LuaScripting::lua_imGuiShow(lua_State* L) {
    if (instance && instance->setImGuiVisibleFunc) {
        instance->setImGuiVisibleFunc(true);
    }
    return 0;
}
