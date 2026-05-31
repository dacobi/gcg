#include "godot_manager.h"
#include "core/extension/libgodot.h"
#include "core/extension/godot_instance.h"
#include "servers/display/display_server.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "core/os/os.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "resource_format_loader_gltf.h"
#include "resource_format_loader_wav.h"
#include <iostream>


#include "scene/main/node.h"
#include "servers/audio/audio_stream.h"
#include "scene/audio/audio_stream_player.h"
#include "servers/audio/audio_effect.h"
#include "servers/audio/audio_server.h"
#include <vector>

extern "C" void gcg_audio_mix(float* interleaved_buffer, int frames);
extern "C" void gcg_video_record_audio(const int16_t* pcm_data, int frames);

extern "C" int gcg_get_godot_mix_rate() {
    if (AudioServer::get_singleton()) {
        return AudioServer::get_singleton()->get_mix_rate();
    }
    return 48000;
}

class AudioEffectInstanceGCG : public AudioEffectInstance {
    GDCLASS(AudioEffectInstanceGCG, AudioEffectInstance);
public:
    virtual void process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) override {
        // Pass audio through transparently
        for (int i = 0; i < p_frame_count; i++) {
            p_dst_frames[i] = p_src_frames[i];
        }

        // Intercept and push to the video encoder
        std::vector<int16_t> pcm(p_frame_count * 2);
        const float* f_ptr = (const float*)p_src_frames;
        for (int i = 0; i < p_frame_count * 2; ++i) {
            pcm[i] = static_cast<int16_t>(std::max(-1.0f, std::min(1.0f, f_ptr[i])) * 32767.0f);
        }
        gcg_video_record_audio(pcm.data(), p_frame_count);
    }
};

class AudioEffectGCG : public AudioEffect {
    GDCLASS(AudioEffectGCG, AudioEffect);
public:
    virtual Ref<AudioEffectInstance> instantiate() override {
        Ref<AudioEffectInstanceGCG> ins = memnew(AudioEffectInstanceGCG);
        return ins;
    }
};

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


    ClassDB::register_class<LuaEventBridge>();
    ClassDB::register_class<AudioStreamGCG>();
    ClassDB::register_class<AudioStreamPlaybackGCG>();
    ClassDB::register_class<AudioEffectGCG>();
    ClassDB::register_class<AudioEffectInstanceGCG>();

    SceneTree* tree = SceneTree::get_singleton();
    if (tree && tree->get_root()) {
        AudioStreamPlayer* player = memnew(AudioStreamPlayer);
        Ref<AudioStreamGCG> stream = memnew(AudioStreamGCG);
        player->set_stream(stream);
        player->set_name("GCG_AudioBridge");
        tree->get_root()->add_child(player);
        player->play();
        
        // Attach capture effect to Master bus
        Ref<AudioEffectGCG> capture_effect = memnew(AudioEffectGCG);
        int master_idx = AudioServer::get_singleton()->get_bus_index("Master");
        AudioServer::get_singleton()->add_bus_effect(master_idx, capture_effect);
        
        std::printf("GCG_AudioBridge player and capture effect added.\\n");
    } else {
        std::printf("ERROR: SceneTree or root is null.\\n");
    }


    is_running = true;    return true;
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
        // destroy_godot_instance safely tears it down
        libgodot_destroy_godot_instance(static_cast<GDExtensionObjectPtr>(godot_instance));
        godot_instance = nullptr;
        is_running = false;
    }
}
