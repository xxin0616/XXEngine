#pragma once
#ifndef _XXENGINE_RASTERIZER_FEATURE_H_
#define _XXENGINE_RASTERIZER_FEATURE_H_

#include <string>

namespace RasterizerFeature
{
	bool Initialize();

	int GetShaderOptionCount();
	const char* GetShaderOptionName(int index);
	void SetShaderOption(int index);
	int GetShaderOption();
	int GetModelOptionCount();
	const char* GetModelOptionName(int index);
	std::string GetModelOptionLoaderName(int index);
	std::string GetModelOptionPath(int index);
	std::string GetModelOptionPrimaryTexturePath(int index);
	void GetModelOptionOpenGLRotationDeg(int index, float& rx, float& ry, float& rz);
	void SetModelOption(int index);
	int GetModelOption();

	void RenderInImGuiChild();
	bool SaveCurrentPng(const std::string& output_path);
	bool IsRendering();
	void Shutdown();
}

#endif // !_XXENGINE_RASTERIZER_FEATURE_H_
