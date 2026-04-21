#version 450

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragWorldNormal;
layout(location = 2) in vec2 fragUV;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} cam;

layout(push_constant) uniform Push {
    mat4  model;
    vec4  lightDir;
    float metallicFactor;
    float roughnessFactor;
} push;

layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D metallicRoughnessMap;

layout(set = 2, binding = 0) uniform samplerCube irradianceMap;
layout(set = 2, binding = 1) uniform samplerCube prefilteredMap;
layout(set = 2, binding = 2) uniform sampler2D   brdfLUT;

layout(location = 0) out vec4 outColor;

const float PI      = 3.14159265359;
const float MAX_LOD = 4.0; 

const vec3 lightColor = vec3(3.0);

float DistributionGGX(vec3 N, vec3 H, float a)
{
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float k)
{
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float k)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, k) * GeometrySchlickGGX(NdotL, k);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

//Fresnel for IBL ambient 
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{
    vec3  albedo    = texture(albedoMap, fragUV).rgb;
    vec4  mrSample  = texture(metallicRoughnessMap, fragUV);
    float roughness = mrSample.g * push.roughnessFactor;
    float metallic  = mrSample.b * push.metallicFactor;

    vec3 N = normalize(fragWorldNormal);
    vec3 camPos = -transpose(mat3(cam.view)) * vec3(cam.view[3]);
    vec3 V = normalize(camPos - fragWorldPos);
    vec3 R = reflect(-V, N);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

	//directional light
    vec3 L = normalize(push.lightDir.xyz);
    vec3 H = normalize(V + L);

    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, k);
    vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3  kD_direct = (vec3(1.0) - F) * (1.0 - metallic);
    float NdotL     = max(dot(N, L), 0.0);
    vec3  specDirect = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001);
    vec3  Lo         = (kD_direct * albedo / PI + specDirect) * lightColor * NdotL;

    //ibl amb
    float NdotV  = max(dot(N, V), 0.0);
    vec3  kS_ibl = FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3  kD_ibl = (1.0 - kS_ibl) * (1.0 - metallic);

    vec3 iblN = vec3(N.x, -N.y, N.z);
    vec3 iblR = vec3(R.x, -R.y, R.z);

    vec3 irradiance = texture(irradianceMap, iblN).rgb;
    vec3 diffuse    = kD_ibl * irradiance * albedo;

    vec3  prefilteredColor = textureLod(prefilteredMap, iblR, roughness * MAX_LOD).rgb;
    vec2  brdf             = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    vec3  specIBL          = prefilteredColor * (kS_ibl * brdf.x + brdf.y);

    vec3 ambient = diffuse + specIBL;
    vec3 color   = ambient + Lo;

    // Reinhard tone map 
    color = color / (color + vec3(1.0));

    outColor = vec4(color, 1.0);
}
