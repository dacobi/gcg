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
    using SelectFunc = std::function<void(bool isPlasma, int index)>;
    using SetParamFunc = std::function<void(bool isPlasma, const std::string& name, double value)>;
    using RandomizeFunc = std::function<void(bool isPlasma, bool isXY)>;
    using SetAudioFunc = std::function<void(const std::string&)>;
    using RecordFunc = std::function<void(int type, const std::string& path, int val)>;
    using IsRecordingFunc = std::function<bool()>;
    using SelectUSDFunc = std::function<void(int index)>;
    using SetUSDParamFunc = std::function<void(const std::string& name, double value)>;

    LuaScripting(AddBouncerFunc addFunc, DelBouncerFunc delFunc, SetBGFunc setBGFunc, SelectFunc selectFunc, SetParamFunc setParamFunc, RandomizeFunc randomizeFunc, SetAudioFunc setAudioFunc, RecordFunc recordFunc, IsRecordingFunc isRecFunc, SelectUSDFunc selectUSDFunc, SetUSDParamFunc setUSDParamFunc);
    ~LuaScripting();

    bool runScript(const std::string& filename);
    void stop();

private:
    static int lua_addBouncer(lua_State* L);
    static int lua_delBouncer(lua_State* L);
    static int lua_setBG(lua_State* L);
    static int lua_selectPlasma(lua_State* L);
    static int lua_selectFractal(lua_State* L);
    static int lua_selectUSD(lua_State* L);
    static int lua_setPlasmaParam(lua_State* L);
    static int lua_setFractalParam(lua_State* L);
    static int lua_setUSDParam(lua_State* L);
    static int lua_randomizePlasmaPalette(lua_State* L);
    static int lua_randomizePlasmaXY(lua_State* L);
    static int lua_randomizeFractalPalette(lua_State* L);
    static int lua_setAudio(lua_State* L);
    static int lua_startRecord(lua_State* L);
    static int lua_stopRecord(lua_State* L);
    static int lua_setRecordMax(lua_State* L);
    static int lua_delay(lua_State* L);
    static void lua_hook(lua_State* L, lua_Debug* ar);

    void scriptThreadFunc(std::string filename);

    lua_State* L = nullptr;
    std::thread scriptThread;
    std::atomic<bool> running{false};

    AddBouncerFunc addBouncerFunc;
    DelBouncerFunc delBouncerFunc;
    SetBGFunc setBGFunc;
    SelectFunc selectFunc;
    SetParamFunc setParamFunc;
    RandomizeFunc randomizeFunc;
    SetAudioFunc setAudioFunc;
    RecordFunc recordFunc;
    IsRecordingFunc isRecFunc;
    SelectUSDFunc selectUSDFunc;
    SetUSDParamFunc setUSDParamFunc;

    static LuaScripting* instance;
};
