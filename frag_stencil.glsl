#version 450
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inColor;
layout(location = 0) out vec4 outFragColor;

layout(set=2, binding=0) uniform sampler2D tex;
layout(set=2, binding=1) uniform sampler2D stencil_tex;

void main() {
    vec4 c = texture(tex, inUV);
    vec4 s = texture(stencil_tex, inUV);
    c.a *= s.a;
    outFragColor = c * inColor;
}
