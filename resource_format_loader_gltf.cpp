#include "resource_format_loader_gltf.h"
#include "modules/gltf/gltf_document.h"
#include "modules/gltf/gltf_state.h"
#include "scene/resources/packed_scene.h"
#include "scene/main/node.h"

Ref<Resource> ResourceFormatLoaderGLTF::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
    Ref<GLTFDocument> gltf_doc;
    gltf_doc.instantiate();
    Ref<GLTFState> gltf_state;
    gltf_state.instantiate();

    Error err = gltf_doc->append_from_file(p_path, gltf_state);
    if (r_error) {
        *r_error = err;
    }
    
    if (err != OK) {
        return Ref<Resource>();
    }

    Node *scene_node = gltf_doc->generate_scene(gltf_state);
    if (!scene_node) {
        if (r_error) {
            *r_error = ERR_PARSE_ERROR;
        }
        return Ref<Resource>();
    }

    Ref<PackedScene> packed_scene;
    packed_scene.instantiate();
    
    err = packed_scene->pack(scene_node);
    memdelete(scene_node); // Node is no longer needed after packing
    
    if (r_error) {
        *r_error = err;
    }

    if (err != OK) {
        return Ref<Resource>();
    }

    packed_scene->set_path(p_path);
    return packed_scene;
}

void ResourceFormatLoaderGLTF::get_recognized_extensions(List<String> *p_extensions) const {
    p_extensions->push_back("gltf");
    p_extensions->push_back("glb");
}

bool ResourceFormatLoaderGLTF::handles_type(const String &p_type) const {
    return p_type == "PackedScene" || p_type == "Node";
}

String ResourceFormatLoaderGLTF::get_resource_type(const String &p_path) const {
    String ext = p_path.get_extension().to_lower();
    if (ext == "gltf" || ext == "glb") {
        return "PackedScene";
    }
    return "";
}