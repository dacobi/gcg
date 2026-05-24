#ifndef LUASCRIPTING_H
#define LUASCRIPTING_H

#include <lua5.4/lua.hpp>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>

struct LuaSyncData {
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;
    bool b_res = false;
    std::string s_res;
    float f_res[3] = {0.0f, 0.0f, 0.0f};
};

class LuaScripting {
public:
    enum GodotCmd {
        GCMD_SELECT_ROOT,
        GCMD_SELECT_NODE,
        GCMD_SEARCH_NODE,
        GCMD_GET_NODE_TYPE,
        GCMD_GET_NAME,
        GCMD_RENAME_NODE,
        GCMD_SET_CAMERA,
        GCMD_GET_POS,
        GCMD_SET_POS,
        GCMD_MOVE_X,
        GCMD_MOVE_Y,
        GCMD_MOVE_Z,
        GCMD_CREATE_NODE,
        GCMD_LOAD_NODE,
        GCMD_DELETE_NODE
    };

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
    using SelectGodotFunc = std::function<void(int index)>;
    using SetUSDParamFunc = std::function<void(const std::string& name, double value)>;
    using GodotCmdFunc = std::function<void(GodotCmd cmd, const std::string& str_arg, float f_args[3], LuaSyncData* sync_data)>;

    LuaScripting(AddBouncerFunc addFunc, DelBouncerFunc delFunc, SetBGFunc setBGFunc, SelectFunc selectFunc, SetParamFunc setParamFunc, RandomizeFunc randomizeFunc, SetAudioFunc setAudioFunc, RecordFunc recordFunc, IsRecordingFunc isRecFunc, SelectUSDFunc selectUSDFunc, SelectGodotFunc selectGodotFunc, SetUSDParamFunc setUSDParamFunc, GodotCmdFunc godotCmdFunc);
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
    static int lua_selectGodot(lua_State* L);
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
    
    // Input Framework
    static int lua_ioKBClicked(lua_State* L);
    static int lua_ioKBDown(lua_State* L);
    static int lua_ioKBUp(lua_State* L);
    static int lua_ioMousePos(lua_State* L);
    static int lua_ioMouseMoved(lua_State* L);
    static int lua_ioMouseGetMotion(lua_State* L);
    static int lua_ioMouseBTNClicked(lua_State* L);
    static int lua_ioMouseBTNDown(lua_State* L);
    static int lua_ioMouseBTNUp(lua_State* L);

    // Godot Manipulation
    static int lua_godotSelectRoot(lua_State* L);
    static int lua_godotSelectNode(lua_State* L);
    static int lua_godotSearchNode(lua_State* L);
    static int lua_godotGetNodeType(lua_State* L);
    static int lua_godotGetName(lua_State* L);
    static int lua_godotRenameNode(lua_State* L);
    static int lua_godotSetCamera(lua_State* L);
    static int lua_godotGetPos(lua_State* L);
    static int lua_godotSetPos(lua_State* L);
    static int lua_godotMoveX(lua_State* L);
    static int lua_godotMoveY(lua_State* L);
    static int lua_godotMoveZ(lua_State* L);
    static int lua_godotCreateNode(lua_State* L);
    static int lua_godotLoadNode(lua_State* L);
    static int lua_godotDeleteNode(lua_State* L);

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
    SelectGodotFunc selectGodotFunc;
    SetUSDParamFunc setUSDParamFunc;
    GodotCmdFunc godotCmdFunc;

    static LuaScripting* instance;
};

#endif
