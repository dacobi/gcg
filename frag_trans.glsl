#version 450
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inColor;
layout(location = 0) out vec4 outFragColor;

layout(set=2, binding=0) uniform sampler2D tex;

void main() {
    vec4 texColor = texture(tex, inUV);
    
    // Multi-condition check to be more surgical
    // 1. Very dark luminance
    float luma = dot(texColor.rgb, vec3(0.299, 0.587, 0.114));
    // 2. All channels are individually very low (catches dark grey/tinted noise)
    bool allLow = (texColor.r < 0.08 && texColor.g < 0.08 && texColor.b < 0.08);
    
    if (luma < 0.07 || allLow) {
        discard;
    }
    
    outFragColor = texColor * inColor;
}
