#version 450

layout(location = 0) in  vec3 inPos;
layout(location = 0) out vec3 localPos;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} cam;

void main() {
    localPos = inPos;

    mat4 rotView = mat4(mat3(cam.view));
    vec4 clip    = cam.proj * rotView * vec4(inPos, 1.0);
    gl_Position  = clip.xyww;
}
