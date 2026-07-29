#pragma once

#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <functional>
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
    static void update_overlay_texture(int width, int height, void* pixels);
    static void get_mouse_position(float& x, float& y);
    static void get_window_size(int& w, int& h);
    static bool is_mouse_button_pressed(int button);

    static std::function<void(uint32_t unicode, uint32_t keycode, bool pressed)> key_callback;
    static std::function<void(float x, float y)> mouse_pos_callback;
    static std::function<void(int button, bool pressed)> mouse_btn_callback;
    static float get_input_axis(const std::string& neg_action, const std::string& pos_action);
    static bool is_action_pressed(const std::string& action);

private:
    void* godot_instance = nullptr;
    std::thread godot_thread;
    std::atomic<bool> is_running{false};
    Ref<class ResourceFormatLoaderGLTF> gltf_loader;
};
