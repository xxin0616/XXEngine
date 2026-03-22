#include "Include.h"
#include <string>

class ConfigPanel {
public:
    void OnRender();

    static int GetConfig() { return m_backendIndex; }
    int GetFeatureIndex() const { return m_featureIndex; }
    void SetFeatureIndex(int index) { m_featureIndex = index; }
    int GetRasterizerShaderIndex() const { return m_rasterizerShaderIndex; }
    int GetRasterizerModelIndex() const { return m_rasterizerModelIndex; }
    int GetOpenGLModelIndex() const { return m_openglModelIndex; }
    bool ConsumeCaptureRequest(std::string& outputPath);

private:
    std::string GetCurrentFeatureNameEn() const;
    static std::string BuildOutputPath(const std::string& featureNameEn);

    static int m_backendIndex; // 0:Raster 1:OpenGL 2:Vulkan
    int m_featureIndex = 0; // 0:Bezier 1:Blinn-Phong
    int m_rasterizerShaderIndex = 0; // 0:texture 1:normal 2:phong 3:bump 4:displacement
    int m_rasterizerModelIndex = 0;
    int m_openglModelIndex = 0;
    bool m_captureRequested = false;
    std::string m_captureOutputPath;
};
