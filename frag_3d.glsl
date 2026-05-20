#version 450
layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

layout(set = 3, binding = 0) uniform LightData {
    vec3 position;
    float padding1;
    vec3 color;
    float intensity;
    vec3 ambientColor;
    float ambientIntensity;
} light;

layout(set = 3, binding = 1) uniform MaterialData {
    vec3 baseColor;
    float roughness;
    float metallic;
    float hasBaseColorTexture;
    float padding2;
} material;

layout(set = 2, binding = 0) uniform sampler2D baseColorTex;

void main() {
    vec4 texColor = texture(baseColorTex, fragUV);
    
    // Debug: directly output texture
    // If it's still gray, maybe the texture itself is gray or UVs are wrong.
    // If we see a red/green gradient, UVs are working but texture is fully transparent/missing.
    if (texColor.a < 0.01) {
        outColor = vec4(fragUV, 0.0, 1.0);
    } else {
        outColor = vec4(texColor.rgb, 1.0);
    }
}
