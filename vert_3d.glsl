#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;

layout(set = 1, binding = 0) uniform TransformData {
    mat4 model;
    mat4 view;
    mat4 projection;
} transforms;

void main() {
    vec4 worldPos = transforms.model * vec4(inPosition, 1.0);
    fragPos = worldPos.xyz;
    
    fragNormal = mat3(transforms.model) * inNormal;
    
    gl_Position = transforms.projection * transforms.view * worldPos;
}