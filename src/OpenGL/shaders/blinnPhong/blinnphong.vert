#version 120

varying vec3 vNormal;
varying vec3 vPosView;
varying vec2 vUV;

void main()
{
    vec4 posView = gl_ModelViewMatrix * gl_Vertex;
    vPosView = posView.xyz;
    vNormal = normalize(gl_NormalMatrix * gl_Normal);
    vUV = gl_MultiTexCoord0.xy;
    gl_Position = gl_ProjectionMatrix * posView;
}
