#version 450

layout(location = 0) in  vec3 localPos;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform samplerCube envMap;

const float PI          = 3.14159265359;
const float SAMPLE_DELTA = 0.025;

void main() {
    vec3 N = normalize(localPos);

    vec3 up    = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up         = normalize(cross(N, right));

    vec3  irradiance = vec3(0.0);
    float nrSamples  = 0.0;

    for (float phi = 0.0; phi < 2.0 * PI; phi += SAMPLE_DELTA)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += SAMPLE_DELTA)
        {
            vec3 tangentSample = vec3(sin(theta) * cos(phi),
                                      sin(theta) * sin(phi),
                                      cos(theta));
            vec3 sampleVec = tangentSample.x * right
                           + tangentSample.y * up
                           + tangentSample.z * N;

            irradiance += texture(envMap, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }

    outColor = vec4(PI * irradiance / nrSamples, 1.0);
}
