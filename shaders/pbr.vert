#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragWorldNormal;
layout(location = 2) out vec2 fragUV;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} cam;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 lightDir;
} push;

void main() {
    vec4 worldPos   = push.model * vec4(inPos, 1.0);
    fragWorldPos    = worldPos.xyz;

    //corrects normals under non uniform scale
    fragWorldNormal = normalize(mat3(transpose(inverse(push.model))) * inNormal);

    fragUV          = inUV;
    gl_Position     = cam.proj * cam.view * worldPos;
}
