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
    std::string getNodeType(void* owner);
    std::string getName(void* owner);
    int getChildCount(void* owner);
    void printHierarchy();
    void renameNode(const std::string& name, void* owner);
    bool setCamera(void* owner);
    bool getPos(float& x, float& y, float& z, void* owner);
    void setPos(float x, float y, float z, void* owner);
    void setVisible(bool visible, void* owner);
    bool getScale(float& x, float& y, float& z, void* owner);
    void setScale(float x, float y, float z, void* owner);
    void move(float x, float y, float z, void* owner);
    bool moveAndCollide(float x, float y, float z, void* owner);
    std::vector<std::string> getOverlappingAreas(void* owner);

    bool createNode(const std::string& name, void* owner);
    bool loadNode(const std::string& path, void* owner, float x = 0, float y = 0, float z = 0, bool use_pos = false);
    void deleteNode(void* owner);

    Node* getCurrentNode(void* owner) const { 
        auto it = current_nodes.find(owner);
        return it != current_nodes.end() ? it->second : nullptr; 
    }
    void setCurrentNode(Node* n, void* owner) { current_nodes[owner] = n; }

    bool attachScript(const std::string& path, void* owner);
    void setProperty(const std::string& name, const class Variant& value, void* owner);
    class Variant getProperty(const std::string& name, void* owner);
    bool watchSignal(const std::string& signal_name, const std::string& callback_file, void* owner);
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
    std::vector<ObjectID> signal_bridges;

    static int active_instance_count;

    Node* _searchNodeRecursive(Node* current, const std::string& name);
};
