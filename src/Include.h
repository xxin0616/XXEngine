#pragma once
#ifndef _XXENGINE_INCLUDE_H_
#define _XXENGINE_INCLUDE_H_

#include "../lib/ImGui/imgui.h"
#include "../lib/ImGui/imgui_impl_glfw.h"
#include "../lib/ImGui/imgui_impl_opengl2.h"
#include <stdio.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <tchar.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif
#include <GLFW/glfw3.h>

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

//#define STB_IMAGE_IMPLEMENTATION
//#include "../lib/stb_image/stb_image.h"

#endif // !_XXENGINE_INCLUDE_H_
