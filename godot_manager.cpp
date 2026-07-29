#include "godot_manager.h"
#include "core/extension/libgodot.h"
#include "core/extension/godot_instance.h"
#include "servers/display/display_server.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/main/canvas_layer.h"
#include "scene/gui/texture_rect.h"
#include "scene/resources/image_texture.h"
#include "core/os/os.h"
#include "core/io/resource_loader.h"
#include "scene/resources/packed_scene.h"
#include "scene/gui/control.h"
#include "core/input/input_event.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "resource_format_loader_gltf.h"
#include "resource_format_loader_wav.h"
#include "resource_format_loader_png.h"
#include <iostream>


#include "scene/main/node.h"
#include "servers/audio/audio_stream.h"
#include "scene/audio/audio_stream_player.h"
#include "servers/audio/audio_effect.h"
#include "servers/audio/audio_server.h"
#include <vector>
#include <alsa/asoundlib.h>
#include <thread>
#include <atomic>
#include "core/input/input.h"

extern "C" void gcg_audio_mix(float* interleaved_buffer, int frames);
extern "C" void gcg_video_record_audio(const int16_t* pcm_data, int frames);

extern "C" int gcg_get_godot_mix_rate() {
    // The ALSA capture loop and AudioMixer both operate at 48000 Hz.
    // Returning Godot's internal 44100 Hz mix rate causes sample rate mismatch and heap buffer overflows in NvencEncoder.
    return 48000;
}

static std::thread alsa_capture_thread;
static std::atomic<bool> alsa_capture_active{false};

void alsa_capture_loop() {
    int err;
    snd_pcm_t *capture_handle;
    
    // Dynamically grab the default PulseAudio/Pipewire sink and set PULSE_SOURCE to its monitor
    FILE *f = popen("pactl get-default-sink 2>/dev/null", "r");
    char cmd_buffer[256];
    if (f && fgets(cmd_buffer, sizeof(cmd_buffer), f)) {
        cmd_buffer[strcspn(cmd_buffer, "\n")] = 0;
        std::string monitor = std::string(cmd_buffer) + ".monitor";
        setenv("PULSE_SOURCE", monitor.c_str(), 1);
        std::cout << "ALSA Capture: Set PULSE_SOURCE to " << monitor << std::endl;
    }
    if (f) pclose(f);

    if ((err = snd_pcm_open(&capture_handle, "pulse", SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        std::cerr << "Cannot open ALSA capture device: " << snd_strerror(err) << std::endl;
        return;
    }
    
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_hw_params_malloc(&hw_params);
    snd_pcm_hw_params_any(capture_handle, hw_params);
    snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(capture_handle, hw_params, SND_PCM_FORMAT_S16_LE);
    unsigned int rate = 48000;
    snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &rate, 0);
    snd_pcm_hw_params_set_channels(capture_handle, hw_params, 2);
    
    if ((err = snd_pcm_hw_params(capture_handle, hw_params)) < 0) {
        std::cerr << "Cannot set ALSA capture params: " << snd_strerror(err) << std::endl;
        snd_pcm_hw_params_free(hw_params);
        snd_pcm_close(capture_handle);
        return;
    }
    snd_pcm_hw_params_free(hw_params);
    snd_pcm_prepare(capture_handle);

    int frames = 1024;
    std::vector<int16_t> buffer(frames * 2);
    
    while (alsa_capture_active) {
        err = snd_pcm_readi(capture_handle, buffer.data(), frames);
        if (err == -EPIPE) {
            snd_pcm_prepare(capture_handle); // overrun/underrun
        } else if (err > 0) {
            gcg_video_record_audio(buffer.data(), err);
        }
    }
    snd_pcm_close(capture_handle);
}

class AudioStreamPlaybackGCG : public AudioStreamPlaybackResampled {
    GDCLASS(AudioStreamPlaybackGCG, AudioStreamPlaybackResampled);
public:
    virtual void start(double p_from_pos = 0.0) override {}
    virtual void stop() override {}
    virtual bool is_playing() const override { return true; }
    virtual int get_loop_count() const override { return 0; }
    virtual double get_playback_position() const override { return 0.0; }
    virtual void seek(double p_time) override {}
    virtual void tag_used_streams() override {}
    virtual void set_parameter(const StringName &p_name, const Variant &p_value) override {}
    virtual Variant get_parameter(const StringName &p_name) const override { return Variant(); }
    
protected:
    virtual int _mix_internal(AudioFrame *p_buffer, int p_frames) override {
        gcg_audio_mix((float*)p_buffer, p_frames);
        return p_frames;
    }
    virtual float get_stream_sampling_rate() override {
        return 48000.0f; // Match MIXER_SAMPLE_RATE
    }
};

class AudioStreamGCG : public AudioStream {
    GDCLASS(AudioStreamGCG, AudioStream);
public:
    virtual Ref<AudioStreamPlayback> instantiate_playback() override {
        Ref<AudioStreamPlaybackGCG> pb = memnew(AudioStreamPlaybackGCG);
        return pb;
    }
    virtual String get_stream_name() const override { return "AudioStreamGCG"; }
    virtual double get_length() const override { return 0.0; }
    virtual bool is_monophonic() const override { return false; }
};


std::queue<GodotSignalEvent> GodotManager::signal_queue;
std::mutex GodotManager::signal_mutex;

class LuaEventBridge : public Node {
    GDCLASS(LuaEventBridge, Node);
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("on_signal", "callback_file", "lua_ptr"), &LuaEventBridge::on_signal);
    }
public:
    void on_signal(String callback_file, uint64_t lua_ptr) {
        std::lock_guard<std::mutex> lock(GodotManager::signal_mutex);
        GodotSignalEvent ev;
        ev.callback_file = callback_file.utf8().get_data();
        ev.lua_scripting = reinterpret_cast<void*>(lua_ptr);
        GodotManager::signal_queue.push(ev);
    }
};

#include "core/os/keyboard.h"

std::function<void(uint32_t, uint32_t, bool)> GodotManager::key_callback = nullptr;
std::function<void(float, float)> GodotManager::mouse_pos_callback = nullptr;
std::function<void(int, bool)> GodotManager::mouse_btn_callback = nullptr;

class ImGuiInputForwarder : public Control {
    GDCLASS(ImGuiInputForwarder, Control);
protected:
    static void _bind_methods() {}
public:
    virtual void gui_input(const Ref<InputEvent> &p_event) override {
        Ref<InputEventKey> k = p_event;
        if (k.is_valid() && GodotManager::key_callback) {
            GodotManager::key_callback(k->get_unicode(), (uint32_t)k->get_keycode_with_modifiers(), k->is_pressed());
        }
        Ref<InputEventMouseMotion> m = p_event;
        if (m.is_valid() && GodotManager::mouse_pos_callback) {
            Vector2 pos = m->get_position();
            Size2 size = get_size();
            if (size.width > 0 && size.height > 0) {
                GodotManager::mouse_pos_callback(pos.x / size.width, pos.y / size.height);
            }
        }
        Ref<InputEventMouseButton> b = p_event;
        if (b.is_valid() && GodotManager::mouse_btn_callback) {
            GodotManager::mouse_btn_callback((int)b->get_button_index(), b->is_pressed());
        }
    }
};

GodotManager::GodotManager() {
}

GodotManager::~GodotManager() {
    shutdown();
}

static GDExtensionBool dummy_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
    r_initialization->initialize = [](void *userdata, GDExtensionInitializationLevel p_level) {};
    r_initialization->deinitialize = [](void *userdata, GDExtensionInitializationLevel p_level) {};
    r_initialization->minimum_initialization_level = GDEXTENSION_INITIALIZATION_CORE;
    return 1;
}

bool GodotManager::init(int argc, char* argv[]) {
    // libgodot_create_godot_instance takes (int argc, char *argv[], GDExtensionInitializationFunction)
    GDExtensionObjectPtr ptr = libgodot_create_godot_instance(argc, argv, dummy_init);
    if (!ptr) {
        std::cerr << "Failed to create Godot instance" << std::endl;
        return false;
    }
    
    godot_instance = ptr;
    GodotInstance* instance = static_cast<GodotInstance*>(godot_instance);
    
    // We start the instance
    if (!instance->start()) {
        std::cerr << "Failed to start Godot instance" << std::endl;
        libgodot_destroy_godot_instance(ptr);
        godot_instance = nullptr;
        return false;
    }
    
    Ref<ResourceFormatLoaderGLTF> gltf_loader;
    gltf_loader.instantiate();
    ResourceLoader::add_resource_format_loader(gltf_loader);

    Ref<ResourceFormatLoaderWAV> wav_loader;
    wav_loader.instantiate();
    ResourceLoader::add_resource_format_loader(wav_loader);

    Ref<ResourceFormatLoaderPNG> png_loader;
    png_loader.instantiate();
    ResourceLoader::add_resource_format_loader(png_loader);


    ClassDB::register_class<LuaEventBridge>();
    ClassDB::register_class<ImGuiInputForwarder>();
    ClassDB::register_class<AudioStreamGCG>();
    ClassDB::register_class<AudioStreamPlaybackGCG>();

    SceneTree* tree = SceneTree::get_singleton();
    if (tree && tree->get_root()) {
        AudioStreamPlayer* player = memnew(AudioStreamPlayer);
        Ref<AudioStreamGCG> stream = memnew(AudioStreamGCG);
        player->set_stream(stream);
        player->set_name("GCG_AudioBridge");
        tree->get_root()->add_child(player);
        player->play();
        
        // Start ALSA capture thread instead of AudioEffectGCG
        alsa_capture_active = true;
        alsa_capture_thread = std::thread(alsa_capture_loop);
        std::printf("GCG ALSA capture thread started.\n");
    } else {
        std::printf("ERROR: SceneTree or root is null.\n");
    }


    is_running = true;    return true;
}

void GodotManager::load_main_scene(const std::string& path) {
    SceneTree* tree = SceneTree::get_singleton();
    if (tree) {
        String godot_path = String(path.c_str());
        if (!godot_path.begins_with("res://")) {
            godot_path = "res://" + godot_path;
        }
        
        Ref<PackedScene> scene = ResourceLoader::load(godot_path);
        if (scene.is_valid()) {
            Node* current_scene = tree->get_current_scene();
            if (current_scene) {
                tree->get_root()->remove_child(current_scene);
                current_scene->queue_free();
            }
            Node* new_scene = scene->instantiate();
            if (new_scene) {
                tree->get_root()->add_child(new_scene);
                tree->set_current_scene(new_scene);
            }
        }
        
        if (tree->get_root()) {
            std::printf("Godot DisplayServer is: %s\n", DisplayServer::get_singleton()->get_name().utf8().get_data());
            tree->get_root()->set_mode(Window::MODE_WINDOWED);
            tree->get_root()->set_visible(true);
        }
    }
}

void GodotManager::update_overlay_texture(int width, int height, void* pixels) {
    SceneTree* tree = SceneTree::get_singleton();
    if (!tree) return;
    
    Window* root = tree->get_root();
    if (!root) return;
    
    // Find or create CanvasLayer
    CanvasLayer* overlay_layer = Object::cast_to<CanvasLayer>(root->get_node_or_null(NodePath("SDLOverlayLayer")));
    TextureRect* rect = nullptr;
    if (!overlay_layer) {
        overlay_layer = memnew(CanvasLayer);
        overlay_layer->set_name("SDLOverlayLayer");
        overlay_layer->set_layer(128); // high layer
        root->add_child(overlay_layer);
        
        rect = memnew(TextureRect);
        rect->set_name("SDLOverlayRect");
        rect->set_anchors_preset(Control::PRESET_FULL_RECT);
        rect->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
        rect->set_stretch_mode(TextureRect::STRETCH_SCALE);
        overlay_layer->add_child(rect);

        ImGuiInputForwarder* forwarder = memnew(ImGuiInputForwarder);
        forwarder->set_name("ImGuiInputForwarder");
        forwarder->set_anchors_preset(Control::PRESET_FULL_RECT);
        forwarder->set_focus_mode(Control::FOCUS_ALL);
        overlay_layer->add_child(forwarder);
        forwarder->grab_focus();
    } else {
        rect = Object::cast_to<TextureRect>(overlay_layer->get_node_or_null(NodePath("SDLOverlayRect")));
        ImGuiInputForwarder* forwarder = Object::cast_to<ImGuiInputForwarder>(overlay_layer->get_node_or_null(NodePath("ImGuiInputForwarder")));
        if (forwarder && !forwarder->has_focus()) forwarder->grab_focus();
    }
    
    if (rect) {
        static Ref<Image> img;
        if (img.is_null() || img->get_width() != width || img->get_height() != height) {
            img = Image::create_empty(width, height, false, Image::FORMAT_RGBA8);
        }
        memcpy(img->ptrw(), pixels, width * height * 4);
        Ref<ImageTexture> tex = rect->get_texture();
        if (tex.is_valid() && tex->get_width() == width && tex->get_height() == height) {
            tex->update(img);
        } else {
            tex = ImageTexture::create_from_image(img);
            rect->set_texture(tex);
        }
    }
}

void GodotManager::get_mouse_position(float& x, float& y) {
    Input* input = Input::get_singleton();
    if (input) {
        Vector2 pos = input->get_mouse_position();
        x = pos.x;
        y = pos.y;
    } else {
        x = 0; y = 0;
    }
}

void GodotManager::get_window_size(int& w, int& h) {
    SceneTree* tree = SceneTree::get_singleton();
    if (tree && tree->get_root()) {
        Size2i size = tree->get_root()->get_size();
        w = size.width;
        h = size.height;
    } else {
        w = 1; h = 1;
    }
}

bool GodotManager::is_mouse_button_pressed(int button) {
    Input* input = Input::get_singleton();
    if (input) {
        return input->is_mouse_button_pressed((MouseButton)button);
    }
    return false;
}

float GodotManager::get_input_axis(const std::string& neg_action, const std::string& pos_action) {
    Input* input = Input::get_singleton();
    if (input) {
        return input->get_axis(String(neg_action.c_str()), String(pos_action.c_str()));
    }
    return 0.0f;
}

bool GodotManager::is_action_pressed(const std::string& action) {
    Input* input = Input::get_singleton();
    if (input) {
        return input->is_action_pressed(String(action.c_str()));
    }
    return false;
}

void GodotManager::iteration() {
    if (!is_running || !godot_instance) return;
    
    GodotInstance* instance = static_cast<GodotInstance*>(godot_instance);
    bool should_quit = instance->iteration();
    if (should_quit) {
        is_running = false;
    }
}

void GodotManager::shutdown() {
    if (godot_instance) {
        // Stop ALSA thread
        if (alsa_capture_active) {
            alsa_capture_active = false;
            if (alsa_capture_thread.joinable()) {
                alsa_capture_thread.join();
            }
        }
        
        // destroy_godot_instance safely tears it down
        libgodot_destroy_godot_instance(static_cast<GDExtensionObjectPtr>(godot_instance));
        godot_instance = nullptr;
        is_running = false;
    }
}
