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

    int getWidth() const { return width; }
    int getHeight() const { return height; }

private:
    int width;
    int height;
    bool bTransparent;
    
    SubViewport* viewport = nullptr;
    Node* scene_instance = nullptr;
};
