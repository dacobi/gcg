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

    static std::queue<GodotSignalEvent> signal_queue;
    static std::mutex signal_mutex;

private:
    void* godot_instance = nullptr;
    std::thread godot_thread;
    std::atomic<bool> is_running{false};
    Ref<class ResourceFormatLoaderGLTF> gltf_loader;
};
