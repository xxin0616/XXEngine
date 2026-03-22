#version 120

uniform sampler2D uBaseColorTex;
uniform sampler2D uMetalRoughTex;
uniform sampler2D uNormalTex;
uniform sampler2D uAOTex;
uniform sampler2D uEmissiveTex;

uniform int uHasBaseColorTex;
uniform int uHasMetalRoughTex;
uniform int uHasNormalTex;
uniform int uHasAOTex;
uniform int uHasEmissiveTex;

uniform vec3 uLightPosView;
uniform vec3 uLightColor;
uniform vec3 uAmbientIntensity;

varying vec3 vNormal;
varying vec3 vPosView;
varying vec2 vUV;

const float PI = 3.14159265;

float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, 1e-5);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / max(NdotV * (1.0 - k) + k, 1e-5);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

void main()
{
    vec3 N = normalize(vNormal);
    if (uHasNormalTex == 1) {
        // GL2 fixed pipeline path has no tangent basis here; use a mild perturbation.
        vec3 mappedN = normalize(texture2D(uNormalTex, vUV).xyz * 2.0 - 1.0);
        N = normalize(mix(N, mappedN, 0.35));
    }

    vec3 baseColor = vec3(0.86, 0.86, 0.88);
    if (uHasBaseColorTex == 1) {
        baseColor = texture2D(uBaseColorTex, vUV).rgb;
    }

    float metallic = 0.0;
    float roughness = 0.65;
    if (uHasMetalRoughTex == 1) {
        vec3 mr = texture2D(uMetalRoughTex, vUV).rgb;
        metallic = clamp(mr.b, 0.0, 1.0);
        roughness = clamp(mr.g, 0.04, 1.0);
    }

    float ao = 1.0;
    if (uHasAOTex == 1) {
        ao = texture2D(uAOTex, vUV).r;
    }

    vec3 emissive = vec3(0.0);
    if (uHasEmissiveTex == 1) {
        emissive = texture2D(uEmissiveTex, vUV).rgb;
    }

    vec3 V = normalize(-vPosView);
    vec3 L = normalize(uLightPosView - vPosView);
    vec3 H = normalize(V + L);

    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    vec3 F0 = mix(vec3(0.04), baseColor, metallic);
    vec3 F = FresnelSchlick(HdotV, F0);
    float D = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);

    vec3 numerator = D * G * F;
    float denominator = max(4.0 * NdotV * NdotL, 1e-5);
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 radiance = uLightColor;
    vec3 direct = (kD * baseColor / PI + specular) * radiance * NdotL;
    vec3 ambient = uAmbientIntensity * baseColor * ao;

    vec3 color = ambient + direct + emissive;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    gl_FragColor = vec4(color, 1.0);
}
