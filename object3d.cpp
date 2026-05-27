#include "object3d.h"

#ifdef USE_USD

#include <cstring>

Object3D::Object3D() {
    modelMatrix.SetIdentity();
}

Object3D::~Object3D() {
    // Note: User must call destroy(device) manually before destruction,
    // or we'd need to store SDL_GPUDevice pointer here.
}

Object3D::Object3D(Object3D&& other) noexcept {
    vertexBuffer = other.vertexBuffer;
    indexBuffer = other.indexBuffer;
    indexCount = other.indexCount;
    modelMatrix = other.modelMatrix;

    other.vertexBuffer = nullptr;
    other.indexBuffer = nullptr;
    other.indexCount = 0;
}

Object3D& Object3D::operator=(Object3D&& other) noexcept {
    if (this != &other) {
        vertexBuffer = other.vertexBuffer;
        indexBuffer = other.indexBuffer;
        indexCount = other.indexCount;
        modelMatrix = other.modelMatrix;

        other.vertexBuffer = nullptr;
        other.indexBuffer = nullptr;
        other.indexCount = 0;
    }
    return *this;
}

bool Object3D::init(SDL_GPUDevice* device, const std::vector<Vertex3D>& vertices, const std::vector<int>& indices) {
    indexCount = indices.size();
    
    // Create Vertex Buffer
    SDL_GPUBufferCreateInfo vertexBufferInfo = {};
    vertexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vertexBufferInfo.size = vertices.size() * sizeof(Vertex3D);
    vertexBuffer = SDL_CreateGPUBuffer(device, &vertexBufferInfo);
    if (!vertexBuffer) {
        SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
        return false;
    }

    // Create Index Buffer
    SDL_GPUBufferCreateInfo indexBufferInfo = {};
    indexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    indexBufferInfo.size = indices.size() * sizeof(int);
    indexBuffer = SDL_CreateGPUBuffer(device, &indexBufferInfo);
    if (!indexBuffer) {
        SDL_Log("Failed to create index buffer: %s", SDL_GetError());
        return false;
    }

    // Upload data via a transfer command buffer
    SDL_GPUTransferBufferCreateInfo transferBufferInfo = {};
    transferBufferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferBufferInfo.size = vertexBufferInfo.size + indexBufferInfo.size;
    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferBufferInfo);
    
    void* map = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
    memcpy(map, vertices.data(), vertexBufferInfo.size);
    memcpy(static_cast<Uint8*>(map) + vertexBufferInfo.size, indices.data(), indexBufferInfo.size);
    SDL_UnmapGPUTransferBuffer(device, transferBuffer);

    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

    SDL_GPUTransferBufferLocation srcVertLoc = {};
    srcVertLoc.transfer_buffer = transferBuffer;
    srcVertLoc.offset = 0;
    
    SDL_GPUBufferRegion dstVertReg = {};
    dstVertReg.buffer = vertexBuffer;
    dstVertReg.offset = 0;
    dstVertReg.size = vertexBufferInfo.size;
    
    SDL_UploadToGPUBuffer(copyPass, &srcVertLoc, &dstVertReg, false);

    SDL_GPUTransferBufferLocation srcIndLoc = {};
    srcIndLoc.transfer_buffer = transferBuffer;
    srcIndLoc.offset = vertexBufferInfo.size;
    
    SDL_GPUBufferRegion dstIndReg = {};
    dstIndReg.buffer = indexBuffer;
    dstIndReg.offset = 0;
    dstIndReg.size = indexBufferInfo.size;
    
    SDL_UploadToGPUBuffer(copyPass, &srcIndLoc, &dstIndReg, false);

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmdBuf);
    SDL_ReleaseGPUTransferBuffer(device, transferBuffer);

    return true;
}

void Object3D::destroy(SDL_GPUDevice* device) {
    if (vertexBuffer) {
        SDL_ReleaseGPUBuffer(device, vertexBuffer);
        vertexBuffer = nullptr;
    }
    if (indexBuffer) {
        SDL_ReleaseGPUBuffer(device, indexBuffer);
        indexBuffer = nullptr;
    }
    indexCount = 0;
}
#endif
