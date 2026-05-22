#include "godot_renderer.h"
#include "core/os/os.h"
#include "core/object/class_db.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/resources/image_texture.h"
#include "core/io/resource_loader.h"
#include "scene/resources/packed_scene.h"
#include "scene/main/window.h"
#include "servers/rendering/rendering_server.h"
#include <cstring>

GodotRenderer::GodotRenderer(int w, int h, bool transparent) : width(w), height(h), bTransparent(transparent) {
}

GodotRenderer::~GodotRenderer() {
    if (viewport) {
        if (scene_instance) {
            viewport->remove_child(scene_instance);
            memdelete(scene_instance);
        }
        if (viewport->is_inside_tree()) {
            SceneTree* tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
            if (tree && tree->get_root()) {
                tree->get_root()->remove_child(viewport);
            }
        }
        memdelete(viewport);
    }
}

bool GodotRenderer::init(const std::string& tscn_path) {
    SceneTree* tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
    if (!tree || !tree->get_root()) {
        return false;
    }

    viewport = Object::cast_to<SubViewport>(ClassDB::instantiate("SubViewport"));
    if (!viewport) return false;

    viewport->set_size(Size2i(width, height));
    viewport->set_update_mode(SubViewport::UPDATE_ALWAYS);
    viewport->set_transparent_background(bTransparent);
    
    // Add to the main scene tree so it gets processed
    tree->get_root()->add_child(viewport);

    // Load the scene
    Ref<PackedScene> scene = ResourceLoader::load(String(tscn_path.c_str()));
    if (scene.is_valid()) {
        scene_instance = scene->instantiate();
        if (scene_instance) {
            viewport->add_child(scene_instance);
            return true;
        }
    }
    return false;
}

void GodotRenderer::render(uint8_t* out_pixels) {
    if (!viewport) return;
    
    // Ensure Godot has rendered the frame
    RenderingServer::get_singleton()->draw(false);

    // Get the viewport's RID and fetch its texture directly from the server
    RID viewport_rid = viewport->get_viewport_rid();
    RID texture_rid = RenderingServer::get_singleton()->viewport_get_texture(viewport_rid);
    
    if (texture_rid.is_null()) return;

    // Extract the Image directly from the texture RID on the server
    Ref<Image> img = RenderingServer::get_singleton()->texture_2d_get(texture_rid);
    if (img.is_null()) return;

    if (img->get_format() != Image::FORMAT_RGBA8) {
        img->convert(Image::FORMAT_RGBA8);
    }
    
    Vector<uint8_t> data = img->get_data();
    if (data.size() < (size_t)width * height * 4) return;

    const uint8_t* src = data.ptr();
    std::memcpy(out_pixels, src, width * height * 4);
}
