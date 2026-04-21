#version 450

layout(location = 0) in  vec3 localPos;
layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 3) uniform samplerCube environmentMap;

void main() {
    vec3 sampleDir = vec3(localPos.x, -localPos.y, localPos.z);
    vec3 color = texture(environmentMap, sampleDir).rgb;

    
    color = color / (color + vec3(1.0));
    outColor = vec4(color, 1.0);
}
