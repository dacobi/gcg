#include "shplasma.h"
#include "renderer.h"
#include "fsplasmas_spv.h"
#include "vert_spv.h"

#include "randhelp.h"
#include <cstdio>
#include <string>

extern Renderer* g_renderer;

PlasmaShader::PlasmaShader(int w, int h) : width(w), height(h) {
    startTicks = SDL_GetTicks();
}

PlasmaShader::~PlasmaShader() {
    cleanup();
}

bool PlasmaShader::init(SDL_GPUTextureFormat format, int cPlasmaIDX) {
    cleanup();
    targetFormat = format;

    if (cPlasmaIDX == -1) {
        iPlasmaIDX = rand_int(14); // 0 to 13
    } else {
        iPlasmaIDX = cPlasmaIDX;
    }

    SDL_GPUDevice* device = g_renderer->getDevice();
    if (!device) return false;

    // 1. Create Fragment Shader
    SDL_ShaderCross_SPIRV_Info fs_spirv = { fsplasmas_spv, fsplasmas_spv_len, "main", SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT };
    SDL_ShaderCross_GraphicsShaderResourceInfo fs_resources = { 0, 0, 0, 1 }; // 1 uniform buffer
    
    SDL_GPUShader* fs = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(device, &fs_spirv, &fs_resources, 0);
    if (!fs) {
        SDL_Log("Failed to compile plasma fragment shader: %s", SDL_GetError());
        return false;
    }

    // 2. Load Vertex Shader (reuse vert_spv)
    SDL_ShaderCross_SPIRV_Info vs_spirv = { vert_spv, vert_spv_len, "main", SDL_SHADERCROSS_SHADERSTAGE_VERTEX };
    SDL_ShaderCross_GraphicsShaderResourceInfo vs_resources = { 0, 0, 0, 1 }; // 1 uniform block
    SDL_GPUShader* vs = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(device, &vs_spirv, &vs_resources, 0);
    if (!vs) {
        SDL_ReleaseGPUShader(device, fs);
        SDL_Log("Failed to compile plasma vertex shader: %s", SDL_GetError());
        return false;
    }

    // 3. Create Pipeline
    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.vertex_shader = vs;
    pipeline_info.fragment_shader = fs;
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    
    pipeline_info.target_info.num_color_targets = 1;
    SDL_GPUColorTargetDescription color_target = {};
    color_target.format = targetFormat;
    pipeline_info.target_info.color_target_descriptions = &color_target;

    pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);
    if (!pipeline) {
        SDL_Log("Failed to create plasma pipeline: %s", SDL_GetError());
    }

    SDL_ReleaseGPUShader(device, vs);
    SDL_ReleaseGPUShader(device, fs);

    return pipeline != nullptr;
}

void PlasmaShader::resize(int w, int h) {
    width = w;
    height = h;
    init(targetFormat, iPlasmaIDX);
}

struct PlasmaUniforms {
    float data[28]; // 7 * vec4
};

void PlasmaShader::updateTexture(Renderer* renderer, SDL_GPUTexture* tex) {
    if (!tex || !renderer || !pipeline) return;

    SDL_GPUCommandBuffer* cmd = renderer->getCommandBuffer();
    bool own_cmd = false;
    if (!cmd) {
        cmd = SDL_AcquireGPUCommandBuffer(renderer->getDevice());
        own_cmd = true;
    }
    if (!cmd) return;

    float t = (float)((double)(SDL_GetTicks() - startTicks) / 1000.0);
    CLPlasmaParams p;
    { std::lock_guard<std::mutex> lock(dataMutex); p = params; }

    PlasmaUniforms uniforms;
    // vec4 0
    uniforms.data[0] = t;
    uniforms.data[1] = p.drift_amp;
    uniforms.data[2] = p.drift_speed_x;
    uniforms.data[3] = p.drift_speed_y;
    // vec4 1
    uniforms.data[4] = p.rot_speed;
    uniforms.data[5] = p.scale_base_x;
    uniforms.data[6] = p.scale_base_y;
    uniforms.data[7] = p.palette_phase_r;
    // vec4 2
    uniforms.data[8] = p.palette_phase_g;
    uniforms.data[9] = p.palette_phase_b;
    uniforms.data[10] = p.scale_mod_amp;
    uniforms.data[11] = p.scale_mod_speed_x;
    // vec4 3
    uniforms.data[12] = p.scale_mod_speed_y;
    uniforms.data[13] = p.warp_base;
    uniforms.data[14] = p.warp_amp;
    uniforms.data[15] = p.warp_speed;
    // vec4 4
    uniforms.data[16] = p.swirl_dist_mul;
    uniforms.data[17] = p.darken_r;
    uniforms.data[18] = p.darken_g;
    uniforms.data[19] = p.darken_b;
    // vec4 5
    uniforms.data[20] = p.tile_count;
    uniforms.data[21] = (float)width;
    uniforms.data[22] = (float)height;
    uniforms.data[23] = (float)iPlasmaIDX;
    // vec4 6
    uniforms.data[24] = p.noise_smooth;
    uniforms.data[25] = p.noise_rough;
    uniforms.data[26] = p.zoom;
    uniforms.data[27] = 0.0f;

    SDL_GPUColorTargetInfo target_info = {};
    target_info.texture = tex;
    target_info.load_op = SDL_GPU_LOADOP_DONT_CARE;
    target_info.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &target_info, 1, nullptr);
    if (!pass) return;

    SDL_GPUViewport viewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
    SDL_SetGPUViewport(pass, &viewport);

    SDL_BindGPUGraphicsPipeline(pass, pipeline);
    
    SDL_PushGPUFragmentUniformData(cmd, 0, &uniforms, sizeof(uniforms));
    
    struct Transform {
        float projection[16];
        float dst_rect[4];
        float color[4];
    } transform = {};
    // Identity-ish ortho for 0..1 range
    transform.projection[0] = 2.0f; transform.projection[5] = -2.0f; transform.projection[10] = 1.0f; transform.projection[15] = 1.0f;
    transform.projection[12] = -1.0f; transform.projection[13] = 1.0f;
    transform.dst_rect[0] = 0.0f; transform.dst_rect[1] = 0.0f; transform.dst_rect[2] = 1.0f; transform.dst_rect[3] = 1.0f;
    transform.color[0] = 1.0f; transform.color[1] = 1.0f; transform.color[2] = 1.0f; transform.color[3] = 1.0f;
    
    SDL_PushGPUVertexUniformData(cmd, 0, &transform, sizeof(transform));

    SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
    SDL_EndGPURenderPass(pass);

    if (own_cmd) {
        SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
        SDL_WaitForGPUFences(renderer->getDevice(), true, &fence, 1);
        SDL_ReleaseGPUFence(renderer->getDevice(), fence);
    }
}

void PlasmaShader::setArgs(const CLPlasmaParams& p) {
    std::lock_guard<std::mutex> lock(dataMutex);
    params = p;
}

CLPlasmaParams PlasmaShader::getArgs() {
    std::lock_guard<std::mutex> lock(dataMutex);
    return params;
}

void PlasmaShader::cleanup() {
    if (pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(g_renderer->getDevice(), pipeline);
        pipeline = nullptr;
    }
}
