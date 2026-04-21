#version 450

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec3 localPos;

layout(push_constant) uniform Push {
    mat4  viewProj;
    float roughness;
} push;

void main() {
    localPos    = inPos;
    gl_Position = push.viewProj * vec4(inPos, 1.0);
}
