#include "godot_renderer.h"
#include "core/os/os.h"
#include "core/object/class_db.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/resources/image_texture.h"
#include "core/io/resource_loader.h"
#include "scene/resources/packed_scene.h"
#include "scene/main/window.h"
#include "scene/main/node.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/physics/physics_body_3d.h"
#include "scene/3d/physics/area_3d.h"
#include "core/object/object.h"
#include "core/variant/variant.h"
#include "core/os/memory.h"
#include "servers/rendering/rendering_server.h"
#include "modules/gltf/gltf_document.h"
#include "modules/gltf/gltf_state.h"
#include "imgui.h"
#include <cstring>

Node* GodotRenderer::getCurrentNode(void* owner) const {
    if (owner && current_nodes.find(owner) != current_nodes.end()) {
        Node* ptr = current_nodes.at(owner);
        if (ptr) {
            return ptr;
        }
    }
    if (viewport) {
        return viewport;
    }
    SceneTree* tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
    if (tree) {
        return tree->get_root();
    }
    return nullptr;
}

Node* GodotRenderer::resolveTargetNode(void* owner, void* target_node) const {
    if (target_node) {
        return (Node*)target_node;
    }
    return getCurrentNode(owner);
}

GodotRenderer::GodotRenderer(int w, int h, bool transparent) : width(w), height(h), bTransparent(transparent) {
}

GodotRenderer::~GodotRenderer() {
    clearSignalWatchers();
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
    current_nodes.clear();
    SceneTree* tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
    if (!tree || !tree->get_root()) {
        return false;
    }

    viewport = Object::cast_to<SubViewport>(ClassDB::instantiate("SubViewport"));
    if (!viewport) return false;

    viewport->set_size(Size2i(width, height));
    viewport->set_update_mode(SubViewport::UPDATE_ALWAYS);
    viewport->set_transparent_background(bTransparent);
    viewport->set_use_own_world_3d(true);
    
    // Add to the main scene tree so it gets processed
    tree->get_root()->add_child(viewport);

    String path_str = String(tscn_path.c_str());

    if (path_str.get_extension().to_lower() == "glb" || path_str.get_extension().to_lower() == "gltf") {
        Ref<GLTFDocument> gltf_doc;
        gltf_doc.instantiate();
        Ref<GLTFState> gltf_state;
        gltf_state.instantiate();

        Error err = gltf_doc->append_from_file(path_str, gltf_state);
        if (err == OK) {
            scene_instance = gltf_doc->generate_scene(gltf_state);
            if (scene_instance) {
                viewport->add_child(scene_instance);
                
                return true;
            }
        }
        return false;
    }

    // Load the scene as a generic resource
    Ref<PackedScene> scene = ResourceLoader::load(path_str);
    if (scene.is_valid()) {
        scene_instance = scene->instantiate();
        if (scene_instance) {
            viewport->add_child(scene_instance);
            
            return true;
        }
    }
    return false;
}

void GodotRenderer::selectRoot(void* owner) {
    if (scene_instance) {
        current_nodes[owner] = scene_instance;
    } else {
        SceneTree* tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
        if (tree && tree->get_root()) {
            current_nodes[owner] = tree->get_root();
        } else {
            current_nodes.erase(owner);
        }
    }
}

bool GodotRenderer::selectNode(const std::string& name, void* owner) {
    Node* current_node = getCurrentNode(owner);
    if (!current_node) return false;
    String name_str = String(name.c_str());
    for (int i = 0; i < current_node->get_child_count(); i++) {
        Node* child = current_node->get_child(i);
        if (child->get_name() == name_str) {
            current_nodes[owner] = child;
            return true;
        }
    }
    return false;
}

Node* GodotRenderer::_searchNodeRecursive(Node* current, const std::string& name) {
    if (!current) return nullptr;
    String name_str = String(name.c_str());
    if (current->get_name() == name_str) return current;
    for (int i = 0; i < current->get_child_count(); i++) {
        Node* found = _searchNodeRecursive(current->get_child(i), name);
        if (found) return found;
    }
    return nullptr;
}

bool GodotRenderer::searchNode(const std::string& name, void* owner) {
    Node* current_node = getCurrentNode(owner);
    if (!current_node) return false;
    Node* found = _searchNodeRecursive(current_node, name);
    if (found) {
        current_nodes[owner] = found;
        return true;
    }
    return false;
}

std::string GodotRenderer::getNodeType(void* owner, void* target_node) {
    Node* current_node = resolveTargetNode(owner, target_node);
    if (!current_node) return "None";
    return current_node->get_class().utf8().get_data();
}

std::string GodotRenderer::getName(void* owner, void* target_node) {
    Node* current_node = resolveTargetNode(owner, target_node);
    if (!current_node) return "";
    return String(current_node->get_name()).utf8().get_data();
}

int GodotRenderer::getChildCount(void* owner, void* target_node) {
    Node* current_node = resolveTargetNode(owner, target_node);
    if (!current_node) return 0;
    return current_node->get_child_count();
}

static void _print_node_recursive(Node* n, int depth) {
    if (!n) return;
    for (int i=0; i<depth; ++i) std::printf("  ");
    std::printf("- %s (%s)n", String(n->get_name()).utf8().get_data(), n->get_class().utf8().get_data());
    for (int i=0; i<n->get_child_count(); ++i) {
        _print_node_recursive(n->get_child(i), depth+1);
    }
}

void GodotRenderer::printHierarchy() {
    if (scene_instance) {
        std::printf("--- Godot Hierarchy ---n");
        _print_node_recursive(scene_instance, 0);
        std::printf("-----------------------n");
    }
}

void GodotRenderer::renameNode(const std::string& name, void* owner, void* target_node) {
    Node* current_node = resolveTargetNode(owner, target_node);
    if (current_node) {
        current_node->set_name(String(name.c_str()));
    }
}

bool GodotRenderer::setCamera(void* owner) {
    Node* current_node = getCurrentNode(owner);
    if (!current_node) return false;
    Camera3D* cam = Object::cast_to<Camera3D>(current_node);
    if (cam) {
        cam->make_current();
        return true;
    }
    return false;
}

bool GodotRenderer::getPos(float& x, float& y, float& z, void* owner, void* target_node) {
    Node* current_node = resolveTargetNode(owner, target_node);
    if (!current_node) return false;
    Node3D* n3d = Object::cast_to<Node3D>(current_node);
    if (n3d) {
        Vector3 pos = n3d->get_position();
        x = pos.x; y = pos.y; z = pos.z;
        return true;
    }
    return false;
}

void GodotRenderer::setPos(float x, float y, float z, void* owner, void* target_node) {
    Node* current_node = resolveTargetNode(owner, target_node);
    if (!current_node || current_node == scene_instance) return;
    Node3D* n3d = Object::cast_to<Node3D>(current_node);
    if (n3d) {
        n3d->set_position(Vector3(x, y, z));
    }
}

void GodotRenderer::setVisible(bool visible, void* owner, void* target_node) {
    Node* current_node = resolveTargetNode(owner, target_node);
    if (current_node) {
        if (current_node->has_method("set_visible")) {
            current_node->call("set_visible", visible);
        }
    }
}

bool GodotRenderer::getScale(float& x, float& y, float& z, void* owner, void* target_node) {
    Node* current_node = resolveTargetNode(owner, target_node);
    if (!current_node) return false;
    Node3D* n3d = Object::cast_to<Node3D>(current_node);
    if (n3d) {
        Vector3 s = n3d->get_scale();
        x = s.x; y = s.y; z = s.z;
        return true;
    }
    return false;
}

void GodotRenderer::setScale(float x, float y, float z, void* owner, void* target_node) {
    Node* current_node = resolveTargetNode(owner, target_node);
    if (!current_node) return;
    Node3D* n3d = Object::cast_to<Node3D>(current_node);
    if (n3d) {
        n3d->set_scale(Vector3(x, y, z));
    }
}

void GodotRenderer::move(float x, float y, float z, void* owner, void* target_node) {
    Node* current_node = resolveTargetNode(owner, target_node);
    if (!current_node) return;
    Node3D* n3d = Object::cast_to<Node3D>(current_node);
    if (n3d) {
        n3d->translate(Vector3(x, y, z));
    }
}

bool GodotRenderer::moveAndCollide(float x, float y, float z, void* owner, void* target_node) {
    Node* current_node = resolveTargetNode(owner, target_node);
    if (!current_node) return false;
    PhysicsBody3D* body = Object::cast_to<PhysicsBody3D>(current_node);
    if (body) {
        PhysicsServer3D::MotionParameters params;
        params.from = body->get_global_transform();
        params.motion = Vector3(x, y, z);
        PhysicsServer3D::MotionResult result;
        return body->move_and_collide(params, result);
    }
    return false;
}

std::vector<std::string> GodotRenderer::getOverlappingAreas(void* owner, void* target_node) {
    Node* current_node = resolveTargetNode(owner, target_node);
    std::vector<std::string> overlaps;
    if (!current_node) return overlaps;
    Area3D* area = Object::cast_to<Area3D>(current_node);
    if (area) {
        TypedArray<Area3D> overlapping = area->get_overlapping_areas();
        for (int i = 0; i < overlapping.size(); i++) {
            Area3D* other = Object::cast_to<Area3D>(overlapping[i]);
            if (other) {
                overlaps.push_back(String(other->get_name()).utf8().get_data());
            }
        }
    }
    return overlaps;
}

bool GodotRenderer::createNode(const std::string& name, void* owner) {
    Node* current_node = getCurrentNode(owner);
    if (!current_node) return false;
    Node3D* new_node = memnew(Node3D);
    if (new_node) {
        new_node->set_name(String(name.c_str()));
        current_node->add_child(new_node);
        current_nodes[owner] = new_node;
        return true;
    }
    return false;
}

bool GodotRenderer::loadNode(const std::string& path, void* owner, float x, float y, float z, bool use_pos, void* target_node) {
    Node* parent_node = resolveTargetNode(owner, target_node);
    if (!parent_node) return false;
    Ref<PackedScene> scene = ResourceLoader::load(String(path.c_str()));
    if (scene.is_valid()) {
        Node* instance = scene->instantiate();
        if (instance) {
            if (use_pos) {
                Node3D* n3d = Object::cast_to<Node3D>(instance);
                if (n3d) {
                    n3d->set_position(Vector3(x, y, z));
                }
            }
            parent_node->add_child(instance);
            current_nodes[owner] = instance;
            return true;
        }
    }
    return false;
}

void GodotRenderer::deleteNode(void* owner, void* target_node) {
    Node* current_node = resolveTargetNode(owner, target_node);
    if (!current_node || current_node == scene_instance) return;
    Node* to_delete = current_node;
    current_nodes[owner] = to_delete->get_parent();
    if (!getCurrentNode(owner)) current_nodes[owner] = scene_instance;
    to_delete->queue_free();
}

bool GodotRenderer::attachScript(const std::string& path, void* owner) {
    Node* current_node = getCurrentNode(owner);
    if (!current_node) return false;
    Ref<Resource> res = ResourceLoader::load(String(path.c_str()));
    if (res.is_valid()) {
        current_node->set_script(res);
        return true;
    }
    return false;
}

void GodotRenderer::setProperty(const std::string& name, const Variant& value, void* owner, void* target_node) {
    Node* current_node = resolveTargetNode(owner, target_node);
    if (current_node) {
        current_node->set(String(name.c_str()), value);
    }
}

Variant GodotRenderer::getProperty(const std::string& name, void* owner, void* target_node) {
    Node* current_node = resolveTargetNode(owner, target_node);
    if (current_node) {
        return current_node->get(String(name.c_str()));
    }
    return Variant();
}

bool GodotRenderer::watchSignal(const std::string& signal_name, const std::string& callback_file, void* owner_thread, void* owner_engine, void* target_node) {
    Node* current_node = resolveTargetNode(owner_thread, target_node);
    if (!current_node) return false;
    
    Object* bridge_obj = ClassDB::instantiate("LuaEventBridge");
    if (!bridge_obj) return false;
    
    Node* bridge = Object::cast_to<Node>(bridge_obj);
    if (!bridge) {
        memdelete(bridge_obj);
        return false;
    }
    
    current_node->add_child(bridge);
    signal_bridges.push_back(bridge);
    
    Array binds;
    binds.push_back(String(callback_file.c_str()));
    binds.push_back((uint64_t)owner_engine);
    Callable callable(bridge, "on_signal");
    callable = callable.bindv(binds);
    
    Error err = current_node->connect(String(signal_name.c_str()), callable);
    return err == OK;
}

void GodotRenderer::resize(int w, int h) {
    width = w;
    height = h;
    if (viewport) {
        viewport->set_size(Size2i(width, height));
    }
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

static void renderGodotNode(Node* node) {
    if (!node) return;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DefaultOpen;
    if (node->get_child_count() == 0) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    String name = node->get_name();
    String type = node->get_class();
    char label[256];
    std::snprintf(label, sizeof(label), "%s [%s]", name.utf8().get_data(), type.utf8().get_data());

    Camera3D* camera = Object::cast_to<Camera3D>(node);
    bool is_current_camera = camera && camera->is_current();

    if (is_current_camera) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
    }

    bool open = ImGui::TreeNodeEx(label, flags);

    if (is_current_camera) {
        ImGui::PopStyleColor();
    }

    if (ImGui::IsItemClicked() && camera) {
        camera->make_current();
    }

    if (open && node->get_child_count() > 0) {
        for (int i = 0; i < node->get_child_count(); i++) {
            renderGodotNode(node->get_child(i));
        }
        ImGui::TreePop();
    }
}

void GodotRenderer::renderTree() {
    if (scene_instance) {
        renderGodotNode(scene_instance);
    }
}

void GodotRenderer::clearSignalWatchers() {
    for (Node* n : signal_bridges) {
        if (n) {
            if (n->get_parent()) {
                n->get_parent()->remove_child(n);
            }
            memdelete(n);
        }
    }
    signal_bridges.clear();
}

void* GodotRenderer::getNodePointer(const std::string& name, void* owner) {
    Node* current_node = getCurrentNode(owner);
    if (!current_node) return nullptr;
    Node* found = _searchNodeRecursive(current_node, name);
    if (found) {
        return (void*)found;
    }
    return nullptr;
}
