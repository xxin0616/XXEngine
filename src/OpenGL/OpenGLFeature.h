#pragma once

#include "../Include.h"

enum class OpenGLShadingEffect {
    Textured = 0,
    BlinnPhong = 1,
    Pbr = 2
};

class OpenGLFeature {
public:
    static void RenderInImGuiChild(int model_index);
    static void SetShadingEffect(OpenGLShadingEffect effect);
};
