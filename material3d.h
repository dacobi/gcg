#pragma once

#include <pxr/base/gf/vec3f.h>

#include <SDL3/SDL.h>

class Material3D {
public:
    Material3D();
    ~Material3D();
    
    void setBaseColor(const pxr::GfVec3f& color);
    void setEmissiveColor(const pxr::GfVec3f& color);
    void setRoughness(float r);
    void setMetallic(float m);
    void setOcclusion(float o);
    void setBaseColorTexture(SDL_GPUTexture* tex);
    
    struct UniformData {
        float baseColor[4];
        float emissiveColor[4];
        float roughness;
        float metallic;
        float occlusion;
        float hasDiffuseTex;
        float hasEmissiveTex;
        float hasRoughnessTex;
        float hasMetallicTex;
        float padding;
    };
    
    UniformData getUniformData() const;
    SDL_GPUTexture* getBaseColorTexture() const { return baseColorTexture; }

private:
    pxr::GfVec3f baseColor;
    pxr::GfVec3f emissiveColor;
    float roughness;
    float metallic;
    float occlusion;
    SDL_GPUTexture* baseColorTexture = nullptr;
};