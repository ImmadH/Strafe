#version 450

layout(location = 0) in  vec3 localPos;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D equirectMap;

const vec2 INV_ATAN = vec2(0.1591549, 0.3183099); 

vec2 SampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(clamp(v.y, -1.0, 1.0)));
    uv *= INV_ATAN;
    uv += 0.5;
    return uv;
}

void main() {
    vec2 uv  = SampleSphericalMap(normalize(localPos));
    outColor = vec4(texture(equirectMap, uv).rgb, 1.0);
}
