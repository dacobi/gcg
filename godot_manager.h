#pragma once

#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include "core/object/ref_counted.h"

struct GodotSignalEvent {
    std::string callback_file;
    void* lua_scripting = nullptr;
};

class GodotManager {
public:
    GodotManager();
    ~GodotManager();

    bool init(int argc, char* argv[]);
    void iteration();
    void shutdown();
    bool isRunning() const { return is_running; }

    static std::queue<GodotSignalEvent> signal_queue;
    static std::mutex signal_mutex;

    static void load_main_scene(const std::string& path);
    static float get_input_axis(const std::string& neg_action, const std::string& pos_action);
    static bool is_action_pressed(const std::string& action);

private:
    void* godot_instance = nullptr;
    std::thread godot_thread;
    std::atomic<bool> is_running{false};
    Ref<class ResourceFormatLoaderGLTF> gltf_loader;
};
