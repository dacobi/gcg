#version 450
layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

layout(set = 3, binding = 0) uniform LightData {
    vec3 position;
    float padding1;
    vec3 color;
    float intensity;
} light;

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(light.position - fragPos);
    
    vec3 ambient = 0.1 * light.color * light.intensity;
    
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * light.color * light.intensity;
    
    vec3 objectColor = vec3(0.8, 0.8, 0.8);
    vec3 result = (ambient + diffuse) * objectColor;
    
    outColor = vec4(result, 1.0);
}