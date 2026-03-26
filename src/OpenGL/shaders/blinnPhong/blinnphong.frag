#version 330 core

out vec4 FragColor;

uniform sampler2D uDiffuseTex;
uniform int uUseTexture;
uniform vec3 uAmbientColor;
uniform vec3 uDiffuseColor;
uniform vec3 uSpecularColor;
uniform float uShininess;
uniform vec3 uLightPosView;

in vec3 vNormal;
in vec3 vPosView;
in vec2 vUV;

void main()
{
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightPosView - vPosView);
    vec3 V = normalize(-vPosView);
    vec3 H = normalize(L + V);

    vec3 albedo = uDiffuseColor;
    if (uUseTexture == 1) {
        albedo *= texture(uDiffuseTex, vUV).rgb;
    }

    float ndotl = max(dot(N, L), 0.0);
    float specPow = (ndotl > 0.0) ? pow(max(dot(N, H), 0.0), uShininess) : 0.0;

    vec3 color = uAmbientColor * albedo + ndotl * albedo + specPow * uSpecularColor;
    FragColor = vec4(color, 1.0);
}
