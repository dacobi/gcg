#include "godot_manager.h"
#include "core/extension/libgodot.h"
#include "core/extension/godot_instance.h"
#include "servers/display/display_server.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "core/os/os.h"
#include "resource_format_loader_gltf.h"
#include <iostream>

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
    
    is_running = true;
    return true;
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
