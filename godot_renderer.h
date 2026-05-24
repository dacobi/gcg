#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Forward declarations for Godot classes
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

    void selectRoot();
    bool selectNode(const std::string& name);
    bool searchNode(const std::string& name);
    std::string getNodeType();
    std::string getName();
    int getChildCount();
    void printHierarchy();
    void renameNode(const std::string& name);
    bool setCamera();
    bool getPos(float& x, float& y, float& z);
    void setPos(float x, float y, float z);
    void move(float x, float y, float z);
    bool moveAndCollide(float x, float y, float z);
    std::vector<std::string> getOverlappingAreas();

    bool createNode(const std::string& name);
    bool loadNode(const std::string& path, float x = 0, float y = 0, float z = 0, bool use_pos = false);
    void deleteNode();

    Node* getCurrentNode() const { return current_node; }
    void setCurrentNode(Node* n) { current_node = n; }

    bool attachScript(const std::string& path);
    void setProperty(const std::string& name, const class Variant& value);
    class Variant getProperty(const std::string& name);

    int getWidth() const { return width; }
    int getHeight() const { return height; }

private:
    int width;
    int height;
    bool bTransparent;
    
    SubViewport* viewport = nullptr;
    Node* scene_instance = nullptr;
    Node* current_node = nullptr;

    Node* _searchNodeRecursive(Node* current, const std::string& name);
};
