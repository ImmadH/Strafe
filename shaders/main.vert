#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragUV;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} cam;

layout(push_constant) uniform Push {
    mat4 model;
} push;

void main() {
    gl_Position = cam.proj * cam.view * push.model * vec4(inPos, 1.0);
    fragNormal = inNormal;
    fragUV     = inUV;
}
