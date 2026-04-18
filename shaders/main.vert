#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} cam;

layout(push_constant) uniform Push {
    mat4 model;
} push;

void main() {
    gl_Position = cam.proj * cam.view * push.model * vec4(inPos, 1.0);
    fragColor = inColor;
}
