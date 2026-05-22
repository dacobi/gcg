#include "godot_manager.h"
#include "core/extension/libgodot.h"
#include "core/extension/godot_instance.h"
#include "servers/display/display_server.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "core/os/os.h"
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
    
    bFirstIteration = true; 
    is_running = true;
    return true;
}

void GodotManager::iteration() {
    if (!is_running || !godot_instance) return;
    
    if (bFirstIteration) {
        // Enforce "stealth" window state via DisplayServer on the first iteration
        DisplayServer* ds = DisplayServer::get_singleton();
        if (ds) {
            // Shrink, move offscreen, and minimize
            ds->window_set_size(Size2i(1, 1));
            ds->window_set_position(Point2i(-10000, -10000));
            ds->window_set_mode(DisplayServerEnums::WINDOW_MODE_MINIMIZED);
            
            // Also make it borderless and transparent to be as invisible as possible
            ds->window_set_flag(DisplayServerEnums::WINDOW_FLAG_BORDERLESS, true);
            ds->window_set_flag(DisplayServerEnums::WINDOW_FLAG_TRANSPARENT, true);
        }

        bFirstIteration = false;
    }

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
