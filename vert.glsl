#version 450
layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 outColor;

layout(set=1, binding=0) uniform Transform {
    mat4 projection;
    vec4 dst_rect;
    vec4 color;
} transform;

void main() {
    vec2 pos[6] = vec2[](
        vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
        vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0)
    );
    vec2 uv[6] = vec2[](
        vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
        vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0)
    );
    
    vec2 p = pos[gl_VertexIndex];
    outUV = uv[gl_VertexIndex];
    
    p.x = transform.dst_rect.x + p.x * transform.dst_rect.z;
    p.y = transform.dst_rect.y + p.y * transform.dst_rect.w;
    
    gl_Position = transform.projection * vec4(p, 0.0, 1.0);
    outColor = transform.color;
}
