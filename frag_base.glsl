#version 450
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inColor;
layout(location = 0) out vec4 outFragColor;

layout(set=2, binding=0) uniform sampler2D tex;

void main() {
    outFragColor = texture(tex, inUV) * inColor;
}
