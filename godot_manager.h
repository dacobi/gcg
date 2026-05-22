#pragma once

#include <vector>
#include <string>
#include <thread>
#include <atomic>

class GodotManager {
public:
    GodotManager();
    ~GodotManager();

    bool init(int argc, char* argv[]);
    void iteration();
    void shutdown();

private:
    void* godot_instance = nullptr;
    std::thread godot_thread;
    std::atomic<bool> is_running{false};
    bool bFirstIteration = true;
};
