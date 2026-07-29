#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Forward declarations for Godot classes
#include <unordered_map>
#include "core/object/object_id.h"

class SubViewport;
class Node;

class GodotRenderer {
public:
    GodotRenderer(int w, int h, bool transparent = false);
    ~GodotRenderer();

    bool init(const std::string& tscn_path);
    void render(uint8_t* out_pixels);
    void renderTree();
    void resize(int w, int h);

    void selectRoot(void* owner);
    bool selectNode(const std::string& name, void* owner);
    bool searchNode(const std::string& name, void* owner);
    void* getNodePointer(const std::string& name, void* owner);
    std::string getNodeType(void* owner, void* target_node = nullptr);
    std::string getName(void* owner, void* target_node = nullptr);
    int getChildCount(void* owner, void* target_node = nullptr);
    void printHierarchy();
    void renameNode(const std::string& name, void* owner, void* target_node = nullptr);
    bool setCamera(void* owner);
    bool getPos(float& x, float& y, float& z, void* owner, void* target_node = nullptr);
    void setPos(float x, float y, float z, void* owner, void* target_node = nullptr);
    void setVisible(bool visible, void* owner, void* target_node = nullptr);
    bool getScale(float& x, float& y, float& z, void* owner, void* target_node = nullptr);
    void setScale(float x, float y, float z, void* owner, void* target_node = nullptr);
    void move(float x, float y, float z, void* owner, void* target_node = nullptr);
    bool moveAndCollide(float x, float y, float z, void* owner, void* target_node = nullptr);
    std::vector<std::string> getOverlappingAreas(void* owner, void* target_node = nullptr);

    bool createNode(const std::string& name, void* owner);
    bool loadNode(const std::string& path, void* owner, float x = 0, float y = 0, float z = 0, bool use_pos = false, void* target_node = nullptr);
    void deleteNode(void* owner, void* target_node = nullptr);

    Node* getCurrentNode(void* owner) const;

    Node* resolveTargetNode(void* owner, void* target_node) const;

    void setCurrentNode(Node* n, void* owner) { current_nodes[owner] = n; }

    bool attachScript(const std::string& path, void* owner);
    void setProperty(const std::string& name, const class Variant& value, void* owner, void* target_node = nullptr);
    class Variant getProperty(const std::string& name, void* owner, void* target_node = nullptr);
    bool watchSignal(const std::string& signal_name, const std::string& callback_file, void* owner_thread, void* owner_engine, void* target_node = nullptr);
    void clearSignalWatchers();

    int getWidth() const { return width; }
    int getHeight() const { return height; }

    static int get_active_instance_count() { return active_instance_count; }

private:
    int width;
    int height;
    bool bTransparent;
    
    SubViewport* viewport = nullptr;
    Node* scene_instance = nullptr;
    std::unordered_map<void*, Node*> current_nodes;
    std::vector<Node*> signal_bridges;

    static int active_instance_count;

    Node* _searchNodeRecursive(Node* current, const std::string& name);
};
