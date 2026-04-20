#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragUV;

layout(set = 1, binding = 0) uniform sampler2D albedo;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(albedo, fragUV);
}
