#pragma once

#include <SDL3/SDL.h>
#include <vector>

struct Light3D {
    float position[3];
    float padding1;
    float color[3];
    float intensity;
};

struct Vertex3D {
    float pos[3];
    float normal[3];
    float uv[2];
};

#ifdef USE_USD

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/matrix4f.h>

class Object3D {
public:
    Object3D();
    ~Object3D();

    // Prevent copying because of GPU resources
    Object3D(const Object3D&) = delete;
    Object3D& operator=(const Object3D&) = delete;
    
    Object3D(Object3D&& other) noexcept;
    Object3D& operator=(Object3D&& other) noexcept;

    bool init(SDL_GPUDevice* device, const std::vector<Vertex3D>& vertices, const std::vector<int>& indices);
    void destroy(SDL_GPUDevice* device);

    SDL_GPUBuffer* vertexBuffer = nullptr;
    SDL_GPUBuffer* indexBuffer = nullptr;
    Uint32 indexCount = 0;
    
    pxr::GfMatrix4f modelMatrix;
};

#endif
