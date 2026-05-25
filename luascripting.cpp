#include "luascripting.h"
#include "input_manager.h"
#include <iostream>
#include <chrono>
#include <algorithm>

LuaScripting* LuaScripting::instance = nullptr;

LuaScripting::LuaScripting(AddBouncerFunc addFunc, DelBouncerFunc delFunc, SetBGFunc bgFunc, SelectFunc selectFunc, SetParamFunc setParamFunc, RandomizeFunc randomizeFunc, SetAudioFunc audioFunc, RecordFunc recordFunc, IsRecordingFunc isRecFunc, SelectUSDFunc selectUSDFunc, SelectGodotFunc selectGodotFunc, SetUSDParamFunc setUSDParamFunc, GodotCmdFunc godotFunc, QuitFunc quitFunc, SetImGuiVisibleFunc setImGuiVisibleFunc, ClearAndRunFunc clearAndRunFunc, SetMouseCaptureFunc setMouseCaptureFunc)
    : addBouncerFunc(addFunc), delBouncerFunc(delFunc), setBGFunc(bgFunc), selectFunc(selectFunc), setParamFunc(setParamFunc), randomizeFunc(randomizeFunc), setAudioFunc(audioFunc), recordFunc(recordFunc), isRecFunc(isRecFunc), selectUSDFunc(selectUSDFunc), selectGodotFunc(selectGodotFunc), setUSDParamFunc(setUSDParamFunc), godotCmdFunc(godotFunc), quitFunc(quitFunc), setImGuiVisibleFunc(setImGuiVisibleFunc), clearAndRunFunc(clearAndRunFunc), setMouseCaptureFunc(setMouseCaptureFunc) {
    instance = this;
    systemRunning = true;
}

LuaScripting::~LuaScripting() {
    stop();
    if (instance == this) instance = nullptr;
}

bool LuaScripting::runScript(const std::string& filename) {
    if (primaryRunning) {
        std::cerr << "Script already running" << std::endl;
        return false;
    }
    systemRunning = true;
    primaryRunning = true;
    scriptThread = std::thread(&LuaScripting::scriptThreadFunc, this, filename);
    return true;
}

void LuaScripting::stop() {
    systemRunning = false;
    if (scriptThread.joinable()) {
        scriptThread.join();
    }
    
    std::lock_guard<std::mutex> lock(threadsMutex);
    for (auto& t : detachedThreads) {
        if (t.joinable()) t.join();
    }
    detachedThreads.clear();
    primaryRunning = false;
}

void LuaScripting::scriptThreadFunc(std::string filename) {
    L = luaL_newstate();
    luaL_openlibs(L);

    // Store 'this' in registry for the hook
    lua_pushlightuserdata(L, this);
    lua_setfield(L, LUA_REGISTRYINDEX, "LuaScriptingInstance");

    registerFunctions(L);

    // Set a hook to abort execution if stop() is called
    lua_sethook(L, lua_hook, LUA_MASKCOUNT, 100);

    if (luaL_dofile(L, filename.c_str()) != LUA_OK) {
        std::string err = lua_tostring(L, -1);
        if (err != "Script terminated" && err != "Script aborted") {
            std::cerr << "Lua Error: " << err << std::endl;
        }
    }

    lua_close(L);
    L = nullptr;
}

void LuaScripting::runOneShotScript(const std::string& filename) {
    pruneThreads();
    std::lock_guard<std::mutex> lock(threadsMutex);
    detachedThreads.emplace_back([this, filename]() {
        lua_State* L_one = luaL_newstate();
        luaL_openlibs(L_one);

        // Store 'this' in registry for the hook
        lua_pushlightuserdata(L_one, this);
        lua_setfield(L_one, LUA_REGISTRYINDEX, "LuaScriptingInstance");

        registerFunctions(L_one);
        lua_sethook(L_one, lua_hook, LUA_MASKCOUNT, 100);
        if (luaL_dofile(L_one, filename.c_str()) != LUA_OK) {
            std::string err = lua_tostring(L_one, -1);
            if (err != "Script terminated" && err != "Script aborted") {
                std::cerr << "Lua One-Shot Error: " << err << std::endl;
            }
        }
        lua_close(L_one);
    });
}

void LuaScripting::pruneThreads() {
    std::lock_guard<std::mutex> lock(threadsMutex);
}

void LuaScripting::triggerCallback(const std::string& name) {
    std::lock_guard<std::mutex> lock(callbackMutex);
    pendingCallbacks.push(name);
}

void LuaScripting::registerFunctions(lua_State* L_reg) {
    auto reg = [L_reg, this](const char* name, lua_CFunction func) {
        lua_pushlightuserdata(L_reg, this);
        lua_pushcclosure(L_reg, func, 1);
        lua_setglobal(L_reg, name);
    };

    reg("addBouncer", lua_addBouncer);
    reg("delBouncer", lua_delBouncer);
    reg("setBG", lua_setBG);
    reg("selectPlasma", lua_selectPlasma);
    reg("selectFractal", lua_selectFractal);
    reg("selectUSD", lua_selectUSD);
    reg("selectGodot", lua_selectGodot);
    reg("setPlasmaParam", lua_setPlasmaParam);
    reg("setFractalParam", lua_setFractalParam);
    reg("setUSDParam", lua_setUSDParam);
    reg("randomizePlasmaPalette", lua_randomizePlasmaPalette);
    reg("randomizePlasmaXY", lua_randomizePlasmaXY);
    reg("randomizeFractalPalette", lua_randomizeFractalPalette);
    reg("setAudio", lua_setAudio);
    reg("startRecord", lua_startRecord);
    reg("stopRecord", lua_stopRecord);
    reg("setRecordMax", lua_setRecordMax);
    reg("delay", lua_delay);
    reg("appQuit", lua_appQuit);
    reg("luaClearAndRun", lua_luaClearAndRun);
    reg("imGuiHide", lua_imGuiHide);
    reg("imGuiShow", lua_imGuiShow);
    reg("ioMouseCapture", lua_ioMouseCapture);
    reg("ioMouseRelease", lua_ioMouseRelease);

    // Input Framework
    reg("ioKBClicked", lua_ioKBClicked);
    reg("ioKBDown", lua_ioKBDown);
    reg("ioKBUp", lua_ioKBUp);
    reg("ioMousePos", lua_ioMousePos);
    reg("ioMouseMoved", lua_ioMouseMoved);
    reg("ioMouseGetMotion", lua_ioMouseGetMotion);
    reg("ioMouseBTNClicked", lua_ioMouseBTNClicked);
    reg("ioMouseBTNDown", lua_ioMouseBTNDown);
    reg("ioMouseBTNUp", lua_ioMouseBTNUp);

    // Godot Manipulation
    reg("godotSelectRoot", lua_godotSelectRoot);
    reg("godotSelectNode", lua_godotSelectNode);
    reg("godotSearchNode", lua_godotSearchNode);
    reg("godotGetNodeType", lua_godotGetNodeType);
    reg("godotGetName", lua_godotGetName);
    reg("godotGetChildCount", lua_godotGetChildCount);
    reg("godotPrintHierarchy", lua_godotPrintHierarchy);
    reg("godotRenameNode", lua_godotRenameNode);
    reg("godotSetCamera", lua_godotSetCamera);
    reg("godotGetPos", lua_godotGetPos);
    reg("godotSetPos", lua_godotSetPos);
    reg("godotMoveX", lua_godotMoveX);
    reg("godotMoveY", lua_godotMoveY);
    reg("godotMoveZ", lua_godotMoveZ);
    reg("godotMoveAndCollide", lua_godotMoveAndCollide);
    reg("godotGetOverlappingAreas", lua_godotGetOverlappingAreas);
    reg("godotCreateNode", lua_godotCreateNode);
    reg("godotLoadNode", lua_godotLoadNode);
    reg("godotDeleteNode", lua_godotDeleteNode);
    reg("godotAttachScript", lua_godotAttachScript);
    reg("godotSetProperty", lua_godotSetProperty);
    reg("godotGetProperty", lua_godotGetProperty);
    reg("godotWatchProperty", lua_godotWatchProperty);
}

void LuaScripting::lua_hook(lua_State* L, lua_Debug* ar) {
    lua_getfield(L, LUA_REGISTRYINDEX, "LuaScriptingInstance");
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, -1);
    lua_pop(L, 1);

    if (self && !self->systemRunning) {
        luaL_error(L, "Script aborted");
    }

    // Process pending callbacks
    if (self) {
        std::string callback;
        {
            std::lock_guard<std::mutex> lock(self->callbackMutex);
            if (!self->pendingCallbacks.empty()) {
                callback = self->pendingCallbacks.front();
                self->pendingCallbacks.pop();
            }
        }
        
        if (!callback.empty()) {
            lua_getglobal(L, callback.c_str());
            if (lua_isfunction(L, -1)) {
                if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                    std::printf("Error in Lua callback '%s': %s\n", callback.c_str(), lua_tostring(L, -1));
                }
            } else {
                lua_pop(L, 1);
            }
        }
    }
}

int LuaScripting::lua_addBouncer(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isstring(L, 1)) {
        std::string syntax = lua_tostring(L, 1);
        if (self && self->addBouncerFunc) {
            self->addBouncerFunc(syntax);
        }
    }
    return 0;
}

int LuaScripting::lua_delBouncer(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isinteger(L, 1)) {
        int index = (int)lua_tointeger(L, 1);
        if (self && self->delBouncerFunc) {
            self->delBouncerFunc(index);
        }
    }
    return 0;
}

int LuaScripting::lua_setBG(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isstring(L, 1)) {
        std::string bg = lua_tostring(L, 1);
        if (self && self->setBGFunc) {
            self->setBGFunc(bg);
        }
    }
    return 0;
}

int LuaScripting::lua_selectPlasma(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isinteger(L, 1)) {
        int index = (int)lua_tointeger(L, 1);
        if (self && self->selectFunc) {
            auto sd = std::make_shared<LuaSyncData>();
            self->selectFunc(true, index, sd);
            std::unique_lock<std::mutex> lock(sd->mtx);
            while (!sd->done && self && self->systemRunning) {
                sd->cv.wait_for(lock, std::chrono::milliseconds(10));
            }
        }
    }
    return 0;
}

int LuaScripting::lua_selectFractal(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isinteger(L, 1)) {
        int index = (int)lua_tointeger(L, 1);
        if (self && self->selectFunc) {
            auto sd = std::make_shared<LuaSyncData>();
            self->selectFunc(false, index, sd);
            std::unique_lock<std::mutex> lock(sd->mtx);
            while (!sd->done && self && self->systemRunning) {
                sd->cv.wait_for(lock, std::chrono::milliseconds(10));
            }
        }
    }
    return 0;
}

int LuaScripting::lua_selectUSD(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isinteger(L, 1)) {
        int index = (int)lua_tointeger(L, 1);
        if (self && self->selectUSDFunc) {
            auto sd = std::make_shared<LuaSyncData>();
            self->selectUSDFunc(index, sd);
            std::unique_lock<std::mutex> lock(sd->mtx);
            while (!sd->done && self && self->systemRunning) {
                sd->cv.wait_for(lock, std::chrono::milliseconds(10));
            }
        }
    }
    return 0;
}

int LuaScripting::lua_selectGodot(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isinteger(L, 1)) {
        int index = (int)lua_tointeger(L, 1);
        if (self && self->selectGodotFunc) {
            auto sd = std::make_shared<LuaSyncData>();
            self->selectGodotFunc(index, sd);
            std::unique_lock<std::mutex> lock(sd->mtx);
            while (!sd->done && self && self->systemRunning) {
                sd->cv.wait_for(lock, std::chrono::milliseconds(10));
            }
        }
    }
    return 0;
}

int LuaScripting::lua_setPlasmaParam(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isstring(L, 1) && lua_isnumber(L, 2)) {
        std::string name = lua_tostring(L, 1);
        double val = lua_tonumber(L, 2);
        if (self && self->setParamFunc) {
            self->setParamFunc(true, name, val);
        }
    }
    return 0;
}

int LuaScripting::lua_setFractalParam(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isstring(L, 1) && lua_isnumber(L, 2)) {
        std::string name = lua_tostring(L, 1);
        double val = lua_tonumber(L, 2);
        if (self && self->setParamFunc) {
            self->setParamFunc(false, name, val);
        }
    }
    return 0;
}

int LuaScripting::lua_setUSDParam(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isstring(L, 1) && lua_isnumber(L, 2)) {
        std::string name = lua_tostring(L, 1);
        double val = lua_tonumber(L, 2);
        if (self && self->setUSDParamFunc) {
            self->setUSDParamFunc(name, val);
        }
    }
    return 0;
}

int LuaScripting::lua_randomizePlasmaPalette(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (self && self->randomizeFunc) self->randomizeFunc(true, false);
    return 0;
}

int LuaScripting::lua_randomizePlasmaXY(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (self && self->randomizeFunc) self->randomizeFunc(true, true);
    return 0;
}

int LuaScripting::lua_randomizeFractalPalette(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (self && self->randomizeFunc) self->randomizeFunc(false, false);
    return 0;
}

int LuaScripting::lua_setAudio(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isstring(L, 1)) {
        std::string path = lua_tostring(L, 1);
        if (self && self->setAudioFunc) self->setAudioFunc(path);
    }
    return 0;
}

int LuaScripting::lua_startRecord(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isstring(L, 1)) {
        std::string path = lua_tostring(L, 1);
        if (self && self->recordFunc) self->recordFunc(0, path, 0);
    }
    return 0;
}

int LuaScripting::lua_stopRecord(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    int wait = 0;
    if (lua_isinteger(L, 1)) wait = (int)lua_tointeger(L, 1);
    if (self && self->recordFunc) self->recordFunc(1, "", wait);
    return 0;
}

int LuaScripting::lua_setRecordMax(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isinteger(L, 1)) {
        int max = (int)lua_tointeger(L, 1);
        if (self && self->recordFunc) self->recordFunc(2, "", max);
    }
    return 0;
}

int LuaScripting::lua_delay(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isinteger(L, 1)) {
        int ms = (int)lua_tointeger(L, 1);
        int remaining = ms;
        while (remaining > 0 && self && self->systemRunning) {
            // Process callbacks while waiting
            lua_Debug ar;
            lua_hook(L, &ar);

            int chunk = std::min(remaining, 16);
            std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
            remaining -= chunk;
        }
    }
    return 0;
}

int LuaScripting::lua_appQuit(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (self && self->quitFunc) {
        self->quitFunc(nullptr); // Non-blocking
    }
    return 0;
}

int LuaScripting::lua_luaClearAndRun(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isstring(L, 1) && self && self->clearAndRunFunc) {
        self->clearAndRunFunc(lua_tostring(L, 1), nullptr); // Non-blocking
    }
    return 0;
}

int LuaScripting::lua_imGuiHide(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (self && self->setImGuiVisibleFunc) {
        self->setImGuiVisibleFunc(false);
    }
    return 0;
}

int LuaScripting::lua_imGuiShow(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (self && self->setImGuiVisibleFunc) {
        self->setImGuiVisibleFunc(true);
    }
    return 0;
}

int LuaScripting::lua_ioMouseCapture(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (self && self->setMouseCaptureFunc) {
        self->setMouseCaptureFunc(true);
    }
    return 0;
}

int LuaScripting::lua_ioMouseRelease(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (self && self->setMouseCaptureFunc) {
        self->setMouseCaptureFunc(false);
    }
    return 0;
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
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (self && self->godotCmdFunc) {
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {0,0,0};
        self->godotCmdFunc(GCMD_SELECT_ROOT, "", fargs, sd, self);
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
    return 0;
}

int LuaScripting::lua_godotSelectNode(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isstring(L, 1) && self && self->godotCmdFunc) {
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {0,0,0};
        self->godotCmdFunc(GCMD_SELECT_NODE, lua_tostring(L, 1), fargs, sd, self);
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_pushboolean(L, sd->b_res);
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotSearchNode(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isstring(L, 1) && self && self->godotCmdFunc) {
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {0,0,0};
        self->godotCmdFunc(GCMD_SEARCH_NODE, lua_tostring(L, 1), fargs, sd, self);
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_pushboolean(L, sd->b_res);
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotGetNodeType(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (self && self->godotCmdFunc) {
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {0,0,0};
        self->godotCmdFunc(GCMD_GET_NODE_TYPE, "", fargs, sd, self);
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_pushstring(L, sd->s_res.c_str());
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotGetName(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (self && self->godotCmdFunc) {
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {0,0,0};
        self->godotCmdFunc(GCMD_GET_NAME, "", fargs, sd, self);
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_pushstring(L, sd->s_res.c_str());
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotGetChildCount(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (self && self->godotCmdFunc) {
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {0,0,0};
        self->godotCmdFunc(GCMD_GET_CHILD_COUNT, "", fargs, sd, self);
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_pushinteger(L, (lua_Integer)sd->d_res);
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotPrintHierarchy(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (self && self->godotCmdFunc) {
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {0,0,0};
        self->godotCmdFunc(GCMD_PRINT_HIERARCHY, "", fargs, sd, self);
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
    return 0;
}

int LuaScripting::lua_godotRenameNode(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isstring(L, 1) && self && self->godotCmdFunc) {
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {0,0,0};
        self->godotCmdFunc(GCMD_RENAME_NODE, lua_tostring(L, 1), fargs, sd, self);
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
    return 0;
}

int LuaScripting::lua_godotSetCamera(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (self && self->godotCmdFunc) {
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {0,0,0};
        self->godotCmdFunc(GCMD_SET_CAMERA, "", fargs, sd, self);
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_pushboolean(L, sd->b_res);
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotGetPos(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (self && self->godotCmdFunc) {
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {0,0,0};
        self->godotCmdFunc(GCMD_GET_POS, "", fargs, sd, self);
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_pushnumber(L, sd->f_res[0]);
        lua_pushnumber(L, sd->f_res[1]);
        lua_pushnumber(L, sd->f_res[2]);
        return 3;
    }
    return 0;
}

int LuaScripting::lua_godotSetPos(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3) && self && self->godotCmdFunc) {
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {(float)lua_tonumber(L, 1), (float)lua_tonumber(L, 2), (float)lua_tonumber(L, 3)};
        self->godotCmdFunc(GCMD_SET_POS, "", fargs, sd, self);
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
    return 0;
}

int LuaScripting::lua_godotMoveX(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isnumber(L, 1) && self && self->godotCmdFunc) {
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {(float)lua_tonumber(L, 1), 0, 0};
        self->godotCmdFunc(GCMD_MOVE_X, "", fargs, sd, self);
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
    return 0;
}

int LuaScripting::lua_godotMoveY(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isnumber(L, 1) && self && self->godotCmdFunc) {
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {0, (float)lua_tonumber(L, 1), 0};
        self->godotCmdFunc(GCMD_MOVE_Y, "", fargs, sd, self);
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
    return 0;
}

int LuaScripting::lua_godotMoveZ(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isnumber(L, 1) && self && self->godotCmdFunc) {
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {0, 0, (float)lua_tonumber(L, 1)};
        self->godotCmdFunc(GCMD_MOVE_Z, "", fargs, sd, self);
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
    return 0;
}

int LuaScripting::lua_godotMoveAndCollide(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3) && self && self->godotCmdFunc) {
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {(float)lua_tonumber(L, 1), (float)lua_tonumber(L, 2), (float)lua_tonumber(L, 3)};
        self->godotCmdFunc(GCMD_MOVE_AND_COLLIDE, "", fargs, sd, self);
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_pushboolean(L, sd->b_res);
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotGetOverlappingAreas(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (self && self->godotCmdFunc) {
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {0,0,0};
        self->godotCmdFunc(GCMD_GET_OVERLAPPING_AREAS, "", fargs, sd, self);
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_newtable(L);
        for (size_t i = 0; i < sd->vs_res.size(); ++i) {
            lua_pushinteger(L, (lua_Integer)i + 1);
            lua_pushstring(L, sd->vs_res[i].c_str());
            lua_settable(L, -3);
        }
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotCreateNode(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isstring(L, 1) && self && self->godotCmdFunc) {
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {0,0,0};
        self->godotCmdFunc(GCMD_CREATE_NODE, lua_tostring(L, 1), fargs, sd, self);
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_pushboolean(L, sd->b_res);
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotLoadNode(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isstring(L, 1) && self && self->godotCmdFunc) {
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {0,0,0};
        int use_pos = 0;
        if (lua_gettop(L) >= 4 && lua_isnumber(L, 2) && lua_isnumber(L, 3) && lua_isnumber(L, 4)) {
            fargs[0] = (float)lua_tonumber(L, 2);
            fargs[1] = (float)lua_tonumber(L, 3);
            fargs[2] = (float)lua_tonumber(L, 4);
            use_pos = 1;
        }
        
        std::string path = lua_tostring(L, 1);
        sd->b_res = (use_pos == 1); 

        self->godotCmdFunc(GCMD_LOAD_NODE, path, fargs, sd, self);
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_pushboolean(L, sd->b_res);
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotDeleteNode(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (self && self->godotCmdFunc) {
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {0,0,0};
        self->godotCmdFunc(GCMD_DELETE_NODE, "", fargs, sd, self);
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
    return 0;
}

int LuaScripting::lua_godotAttachScript(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isstring(L, 1) && self && self->godotCmdFunc) {
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {0,0,0};
        self->godotCmdFunc(GCMD_ATTACH_SCRIPT, lua_tostring(L, 1), fargs, sd, self);
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        lua_pushboolean(L, sd->b_res);
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotSetProperty(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isstring(L, 1) && self && self->godotCmdFunc) {
        std::string name = lua_tostring(L, 1);
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {0,0,0};
        if (lua_isnumber(L, 2)) {
            fargs[0] = (float)lua_tonumber(L, 2);
            fargs[1] = 0; // Number
            self->godotCmdFunc(GCMD_SET_PROPERTY, name, fargs, sd, self);
        } else if (lua_isstring(L, 2)) {
            fargs[1] = 1; // String
            std::string combined = name + "|" + lua_tostring(L, 2);
            self->godotCmdFunc(GCMD_SET_PROPERTY, combined, fargs, sd, self);
        } else if (lua_isboolean(L, 2)) {
            fargs[0] = lua_toboolean(L, 2) ? 1.0f : 0.0f;
            fargs[1] = 2; // Bool
            self->godotCmdFunc(GCMD_SET_PROPERTY, name, fargs, sd, self);
        } else {
            return 0;
        }
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
    return 0;
}

int LuaScripting::lua_godotGetProperty(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isstring(L, 1) && self && self->godotCmdFunc) {
        auto sd = std::make_shared<LuaSyncData>();
        float fargs[3] = {0,0,0};
        self->godotCmdFunc(GCMD_GET_PROPERTY, lua_tostring(L, 1), fargs, sd, self);
        std::unique_lock<std::mutex> lock(sd->mtx);
        while (!sd->done && self && self->systemRunning) {
            sd->cv.wait_for(lock, std::chrono::milliseconds(10));
        }
        if (sd->b_res) { 
            lua_pushnumber(L, sd->d_res);
        } else {
            lua_pushstring(L, sd->s_res.c_str());
        }
        return 1;
    }
    return 0;
}

int LuaScripting::lua_godotWatchProperty(lua_State* L) {
    LuaScripting* self = (LuaScripting*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_isstring(L, 1) && lua_isstring(L, 2) && lua_isstring(L, 4) && self && self->godotCmdFunc) {
        std::string node = lua_tostring(L, 1);
        std::string prop = lua_tostring(L, 2);
        std::string file = lua_tostring(L, 4);
        int mode = 0;
        if (lua_isinteger(L, 5)) mode = (int)lua_tointeger(L, 5);

        float fargs[3] = {0,0,0};
        std::string combined = node + "|" + prop + "|" + file + "|";

        if (lua_isnumber(L, 3)) {
            fargs[0] = (float)lua_tonumber(L, 3);
            fargs[1] = 0; // Number
        } else if (lua_isstring(L, 3)) {
            combined += lua_tostring(L, 3);
            fargs[1] = 1; // String
        } else if (lua_isboolean(L, 3)) {
            fargs[0] = lua_toboolean(L, 3) ? 1.0f : 0.0f;
            fargs[1] = 2; // Bool
        } else {
            return 0;
        }
        
        fargs[2] = (float)mode;
        self->godotCmdFunc(GCMD_WATCH_PROPERTY, combined, fargs, nullptr, self);
    }
    return 0;
}
