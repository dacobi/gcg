#include "material3d.h"

Material3D::Material3D()
    : baseColor(0.8f, 0.8f, 0.8f), 
      emissiveColor(0.0f, 0.0f, 0.0f),
      roughness(0.5f), 
      metallic(0.0f), 
      occlusion(1.0f),
      baseColorTexture(nullptr) {
}

Material3D::~Material3D() {
}

void Material3D::setBaseColor(const pxr::GfVec3f& color) { baseColor = color; }
void Material3D::setEmissiveColor(const pxr::GfVec3f& color) { emissiveColor = color; }
void Material3D::setRoughness(float r) { roughness = r; }
void Material3D::setMetallic(float m) { metallic = m; }
void Material3D::setOcclusion(float o) { occlusion = o; }
void Material3D::setBaseColorTexture(SDL_GPUTexture* tex) { baseColorTexture = tex; }

Material3D::UniformData Material3D::getUniformData() const {
    UniformData data;
    data.baseColor[0] = baseColor[0];
    data.baseColor[1] = baseColor[1];
    data.baseColor[2] = baseColor[2];
    data.baseColor[3] = 1.0f;
    
    data.emissiveColor[0] = emissiveColor[0];
    data.emissiveColor[1] = emissiveColor[1];
    data.emissiveColor[2] = emissiveColor[2];
    data.emissiveColor[3] = 1.0f;
    
    data.roughness = roughness;
    data.metallic = metallic;
    data.occlusion = occlusion;
    data.hasDiffuseTex = baseColorTexture ? 1.0f : 0.0f;
    data.hasEmissiveTex = 0.0f;
    data.hasRoughnessTex = 0.0f;
    data.hasMetallicTex = 0.0f;
    data.padding = 0.0f;
    
    return data;
}
