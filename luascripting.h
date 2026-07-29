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
#include <memory>
#include <map>
#include <set>
#include <utility>

struct LuaSyncData {
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;
    bool b_res = false;
    std::string s_res;
    float f_res[3] = {0.0f, 0.0f, 0.0f};
    double d_res = 0.0;
    std::vector<std::string> vs_res;
    void* ptr_res = nullptr;
    void* ptr_arg = nullptr;
};

class LuaScripting {
public:
    enum GodotCmd {
        GCMD_GET_NODE_POINTER,
        GCMD_SELECT_ROOT,
        GCMD_SELECT_NODE,
        GCMD_SEARCH_NODE,
        GCMD_GET_NODE_TYPE,
        GCMD_GET_NAME,
        GCMD_GET_CHILD_COUNT,
        GCMD_PRINT_HIERARCHY,
        GCMD_RENAME_NODE,
        GCMD_SET_CAMERA,
        GCMD_GET_POS,
        GCMD_SET_POS,
        GCMD_SET_VISIBLE,
        GCMD_GET_SCALE,
        GCMD_SET_SCALE,
        GCMD_MOVE_X,
        GCMD_MOVE_Y,
        GCMD_MOVE_Z,
        GCMD_MOVE_AND_COLLIDE,
        GCMD_GET_OVERLAPPING_AREAS,
        GCMD_CREATE_NODE,
        GCMD_LOAD_NODE,
        GCMD_DELETE_NODE,
        GCMD_ATTACH_SCRIPT,
        GCMD_SET_PROPERTY,
        GCMD_GET_PROPERTY,
        GCMD_WATCH_PROPERTY,
        GCMD_WATCH_SIGNAL
    };

    using AddBouncerFunc = std::function<void(const std::string&)>;
    using DelBouncerFunc = std::function<void(int)>;
    using SetBGFunc = std::function<void(const std::string&)>;
    using SelectFunc = std::function<void(bool isPlasma, int index, std::shared_ptr<LuaSyncData> sync_data)>;
    using SetParamFunc = std::function<void(bool isPlasma, const std::string& name, double value)>;
    using RandomizeFunc = std::function<void(bool isPlasma, bool isXY)>;
    using SetAudioFunc = std::function<void(const std::string&, std::shared_ptr<LuaSyncData>)>;
    using PlayAudioFunc = std::function<void()>;
    using StopAudioFunc = std::function<void()>;
    using RewindAudioFunc = std::function<void()>;
    using SkipAudioFunc = std::function<void(int)>;
    using SetAudioVolumeFunc = std::function<void(int)>;
    using RecordFunc = std::function<void(int type, const std::string& path, int val)>;
    using IsRecordingFunc = std::function<bool()>;
#ifdef USE_USD
    using SelectUSDFunc = std::function<void(int index, std::shared_ptr<LuaSyncData> sync_data)>;
#endif
    using SelectGodotFunc = std::function<void(int index, std::shared_ptr<LuaSyncData> sync_data, void* thread, LuaScripting* engine)>;
#ifdef USE_USD
    using SetUSDParamFunc = std::function<void(const std::string& name, double value)>;
#endif
    using GodotCmdFunc = std::function<void(GodotCmd cmd, const std::string& str_arg, float f_args[3], std::shared_ptr<LuaSyncData> sync_data, void* thread, LuaScripting* engine)>;
    using QuitFunc = std::function<void(std::shared_ptr<LuaSyncData> sync_data)>;
    using SetImGuiVisibleFunc = std::function<void(bool visible)>;
    using ClearAndRunFunc = std::function<void(const std::string& filename, std::shared_ptr<LuaSyncData> sync_data)>;
    using SetMouseCaptureFunc = std::function<void(bool captured)>;
    using SetResizeEnabledFunc = std::function<void(bool enabled)>;
    using MaximizeWindowFunc = std::function<void()>;
    using CheckHighScoreFunc = std::function<bool(int)>;
    using AddHighScoreFunc = std::function<void(const std::string&, int, int)>;
    using LoadHighScoreFunc = std::function<void()>;
    using SaveHighScoreFunc = std::function<void()>;

    LuaScripting(AddBouncerFunc addFunc, DelBouncerFunc delFunc, SetBGFunc setBGFunc, SelectFunc selectFunc, SetParamFunc setParamFunc, RandomizeFunc randomizeFunc, SetAudioFunc setAudioFunc, 
        PlayAudioFunc playAudioFunc, StopAudioFunc stopAudioFunc, RewindAudioFunc rewindAudioFunc, SkipAudioFunc skipAudioFunc, SetAudioVolumeFunc setAudioVolumeFunc,
        RecordFunc recordFunc, IsRecordingFunc isRecFunc,
 
#ifdef USE_USD
        SelectUSDFunc selectUSDFunc, SetUSDParamFunc setUSDParamFunc, 
#endif
        SelectGodotFunc selectGodotFunc, GodotCmdFunc godotCmdFunc, QuitFunc quitFunc, SetImGuiVisibleFunc setImGuiVisibleFunc, ClearAndRunFunc clearAndRunFunc, SetMouseCaptureFunc setMouseCaptureFunc, SetResizeEnabledFunc setResizeEnabledFunc, MaximizeWindowFunc maximizeWindowFunc, CheckHighScoreFunc checkHSFunc, AddHighScoreFunc addHSFunc, LoadHighScoreFunc loadHSFunc, SaveHighScoreFunc saveHSFunc);
    ~LuaScripting();

    bool runScript(const std::string& filename);
    void runOneShotScript(const std::string& filename);
    void stop();
    lua_State* getL() const { return L; }

    void triggerCallback(const std::string& name);

    // Global variables (now managed directly by LuaScripting)
    void setGlobalInt(const std::string& name, int val);
    int getGlobalInt(const std::string& name);
    void regGlobalInt(const std::string& name, int val);
    void unregGlobalInt(const std::string& name);

    void setGlobalFloat(const std::string& name, float val);
    float getGlobalFloat(const std::string& name);
    void regGlobalFloat(const std::string& name, float val);
    void unregGlobalFloat(const std::string& name);

    void loadCarSettings();
    void saveCarSettings();
    void renderLuaImGui();

private:
    static int lua_addBouncer(lua_State* L);
    static int lua_delBouncer(lua_State* L);
    static int lua_setBG(lua_State* L);
    static int lua_godotSingleContext(lua_State* L);
    static int lua_selectPlasma(lua_State* L);
    static int lua_selectFractal(lua_State* L);
#ifdef USE_USD
    static int lua_selectUSD(lua_State* L);
#endif
    static int lua_selectGodot(lua_State* L);
    static int lua_setPlasmaParam(lua_State* L);
    static int lua_setFractalParam(lua_State* L);
#ifdef USE_USD
    static int lua_setUSDParam(lua_State* L);
#endif
    static int lua_randomizePlasmaPalette(lua_State* L);
    static int lua_randomizePlasmaXY(lua_State* L);
    static int lua_randomizeFractalPalette(lua_State* L);
    static int lua_setAudio(lua_State* L);
    static int lua_playAudio(lua_State* L);
    static int lua_stopAudio(lua_State* L);
    static int lua_rewindAudio(lua_State* L);
    static int lua_skipAudio(lua_State* L);
    static int lua_setAudioVolume(lua_State* L);
    static int lua_startRecord(lua_State* L);
    static int lua_stopRecord(lua_State* L);
    static int lua_setRecordMax(lua_State* L);
    static int lua_delay(lua_State* L);
    static int lua_delayKb(lua_State* L);
    static int lua_appQuit(lua_State* L);
    static int lua_luaClearAndRun(lua_State* L);
    static int lua_imGuiHide(lua_State* L);
    static int lua_imGuiShow(lua_State* L);
    static int lua_ioResizeEnabled(lua_State* L);
    static int lua_ioMaximizeWindow(lua_State* L);
    static int lua_ioMouseCapture(lua_State* L);
    static int lua_ioMouseRelease(lua_State* L);
    static int lua_luaCreateMutex(lua_State* L);
    static int lua_luaGetMutex(lua_State* L);
    static int lua_luaTryMutex(lua_State* L);
    static int lua_luaReleaseMutex(lua_State* L);
    static int lua_luaCheckMutex(lua_State* L);
    static int lua_setGlobalVar(lua_State* L);
    static int lua_getGlobalVar(lua_State* L);
    static int lua_regGlobalVar(lua_State* L);
    static int lua_unregGlobalVar(lua_State* L);

    static int lua_setGlobalFloat(lua_State* L);
    static int lua_getGlobalFloat(lua_State* L);
    static int lua_regGlobalFloat(lua_State* L);
    static int lua_unregGlobalFloat(lua_State* L);
    
    // Input Framework
    static int lua_ioKBClicked(lua_State* L);
    static int lua_ioKBDown(lua_State* L);
    static int lua_ioKBUp(lua_State* L);
    static int lua_ioMousePos(lua_State* L);
    static int lua_ioMouseMoved(lua_State* L);
    static int lua_ioMouseGetMotion(lua_State* L);
    static int lua_ioMouseWheelMotion(lua_State* L);
    static int lua_ioMouseBTNClicked(lua_State* L);
    static int lua_ioMouseBTNDown(lua_State* L);
    static int lua_ioMouseBTNUp(lua_State* L);

    // Joystick API
    static int lua_ioJoystickOpen(lua_State* L);
    static int lua_ioJoystickClose(lua_State* L);
    static int lua_ioJoystickGetAxis(lua_State* L);
    static int lua_ioJoystickGetButtonDown(lua_State* L);
    static int lua_ioJoystickGetButtonHit(lua_State* L);
    static int lua_ioJoystickGetButtonUp(lua_State* L);
    static int lua_ioJoystickGetHat(lua_State* L);
    static int lua_ioJoystickGetNumAxes(lua_State* L);
    static int lua_ioJoystickGetNumButtons(lua_State* L);
    static int lua_ioJoystickGetNumHats(lua_State* L);

    // Godot Manipulation
    static int lua_godotGetNodePointer(lua_State* L);
    static int lua_godotSelectRoot(lua_State* L);
    static int lua_godotSelectNode(lua_State* L);
    static int lua_godotInputGetAxis(lua_State* L);
    static int lua_godotInputIsActionPressed(lua_State* L);
    static int lua_godotSearchNode(lua_State* L);
    static int lua_godotGetNodeType(lua_State* L);
    static int lua_godotGetName(lua_State* L);
    static int lua_godotGetChildCount(lua_State* L);
    static int lua_godotPrintHierarchy(lua_State* L);
    static int lua_godotRenameNode(lua_State* L);
    static int lua_godotSetCamera(lua_State* L);
    static int lua_godotGetPos(lua_State* L);
    static int lua_godotSetPos(lua_State* L);
    static int lua_godotSetVisible(lua_State* L);
    static int lua_godotGetScale(lua_State* L);
    static int lua_godotSetScale(lua_State* L);
    static int lua_godotMoveX(lua_State* L);
    static int lua_godotMoveY(lua_State* L);
    static int lua_godotMoveZ(lua_State* L);
    static int lua_godotMoveAndCollide(lua_State* L);
    static int lua_godotGetOverlappingAreas(lua_State* L);
    static int lua_godotCreateNode(lua_State* L);
    static int lua_godotLoadNode(lua_State* L);

    static int lua_godotDeleteNode(lua_State* L);
    static int lua_godotAttachScript(lua_State* L);
    static int lua_godotSetProperty(lua_State* L);
    static int lua_godotGetProperty(lua_State* L);
    static int lua_godotWatchProperty(lua_State* L);
    static int lua_godotWatchSignal(lua_State* L);
    static int lua_godotIsHighScore(lua_State* L);
    static int lua_godotAddHighScore(lua_State* L);
    static int lua_godotLoadHighScore(lua_State* L);
    static int lua_godotSaveHighScore(lua_State* L);
    static int lua_godotLoadCarSettings(lua_State* L);
    static int lua_godotSaveCarSettings(lua_State* L);
    static int lua_godotRegisterImpulseProperty(lua_State* L);

    static void lua_hook(lua_State* L, lua_Debug* ar);

    void scriptThreadFunc(std::string filename);
    void registerFunctions(lua_State* L);
    void pruneThreads();

    lua_State* L = nullptr;
    std::thread scriptThread;
    std::vector<std::thread> detachedThreads;
    std::mutex threadsMutex;
    
    // C++ Property caching maps
    std::set<std::string> impulse_properties;
    std::map<std::pair<void*, std::string>, float> last_float_sets;
    std::map<std::pair<void*, std::string>, std::string> last_string_sets;
    std::map<std::pair<void*, std::string>, bool> last_bool_sets;
    std::atomic<bool> systemRunning{true};
    std::atomic<bool> primaryRunning{false};

    std::queue<std::string> pendingCallbacks;
    std::mutex callbackMutex;

    AddBouncerFunc addBouncerFunc;
    DelBouncerFunc delBouncerFunc;
    SetBGFunc setBGFunc;
    SelectFunc selectFunc;
    SetParamFunc setParamFunc;
    RandomizeFunc randomizeFunc;
    SetAudioFunc setAudioFunc;
    PlayAudioFunc playAudioFunc;
    StopAudioFunc stopAudioFunc;
    RewindAudioFunc rewindAudioFunc;
    SkipAudioFunc skipAudioFunc;
    SetAudioVolumeFunc setAudioVolumeFunc;
    RecordFunc recordFunc;
    IsRecordingFunc isRecFunc;
#ifdef USE_USD
    SelectUSDFunc selectUSDFunc;
    SetUSDParamFunc setUSDParamFunc;
#endif
    SelectGodotFunc selectGodotFunc;
    GodotCmdFunc godotCmdFunc;
    QuitFunc quitFunc;
    SetImGuiVisibleFunc setImGuiVisibleFunc;
    ClearAndRunFunc clearAndRunFunc;
    SetMouseCaptureFunc setMouseCaptureFunc;
    SetResizeEnabledFunc setResizeEnabledFunc;
    MaximizeWindowFunc maximizeWindowFunc;

    CheckHighScoreFunc checkHighScoreFunc;
    AddHighScoreFunc addHighScoreFunc;
    LoadHighScoreFunc loadHighScoreFunc;
    SaveHighScoreFunc saveHighScoreFunc;

    struct ImGuiWidget {
        enum Type { TEXT, SEPARATOR, CHECKBOX, SLIDER_FLOAT, BUTTON, PROGRESS_BAR, SAME_LINE };
        Type type;
        std::string label;
        std::string var_name;
        float min_val = 0.0f;
        float max_val = 0.0f;
    };

    struct ImGuiWindowDef {
        std::string title;
        std::vector<ImGuiWidget> widgets;
    };

    std::vector<ImGuiWindowDef> lua_imgui_windows;
    std::string active_window_title;
    std::mutex imgui_mutex;

    static int lua_imguiBegin(lua_State* L);
    static int lua_imguiEnd(lua_State* L);
    static int lua_imguiText(lua_State* L);
    static int lua_imguiSeparator(lua_State* L);
    static int lua_imguiCheckbox(lua_State* L);
    static int lua_imguiSliderFloat(lua_State* L);
    static int lua_imguiButton(lua_State* L);
    static int lua_imguiProgressBar(lua_State* L);
    static int lua_imguiSameLine(lua_State* L);

    std::unordered_map<std::string, int> global_ints;
    std::unordered_map<std::string, float> global_floats;
    std::mutex globals_mutex;

    std::unordered_map<int, std::unique_ptr<std::recursive_mutex>> dynamic_mutexes;
    int next_mutex_id = 1;

    static LuaScripting* instance;
};

#endif
