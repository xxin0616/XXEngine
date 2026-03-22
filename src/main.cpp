#include "Include.h"
#include "ConfigPanel.h"
#include "bezier/bezier.h"
#include "NotificationCenter.h"
#include "opengl/OpenGLFeature.h"
#include "rasterizer/RasterizerFeature.h"

#include <filesystem>
#include <string>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace {
WNDPROC g_prev_wnd_proc = nullptr;

LRESULT CALLBACK MainWindowProcHook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_CONTEXTMENU ||
		msg == WM_NCRBUTTONDOWN ||
		msg == WM_NCRBUTTONUP)
		return 0;
	if (msg == WM_SYSCOMMAND)
	{
		const WPARAM cmd = (wParam & 0xFFF0);
		if (cmd == SC_MOUSEMENU || cmd == SC_KEYMENU)
			return 0;
	}
	if (g_prev_wnd_proc == nullptr)
		return DefWindowProc(hWnd, msg, wParam, lParam);
	return CallWindowProc(g_prev_wnd_proc, hWnd, msg, wParam, lParam);
}
}
#endif

int main()
{
	if (!glfwInit())
		return 1;

	GLFWwindow* window = glfwCreateWindow(1920, 1080, "XXEngine", NULL, NULL);
	if (window == NULL)
		return 1;

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

#ifdef _WIN32
	HWND hwnd = glfwGetWin32Window(window);
	if (hwnd != nullptr)
		g_prev_wnd_proc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)MainWindowProcHook);
#endif

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	{
		const ImWchar* ranges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
		ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 22.0f, nullptr, ranges);
		if (font == nullptr)
			font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\simhei.ttf", 22.0f, nullptr, ranges);
		if (font == nullptr)
			font = io.Fonts->AddFontDefault();
		io.FontDefault = font;
		io.FontGlobalScale = 1.10f;
	}

	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL2_Init();

	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	ConfigPanel configPanel;
	RasterizerFeature::Initialize();

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		ImGui_ImplOpenGL2_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
		ImGuiWindowFlags window_flags = 0;
		window_flags |= ImGuiWindowFlags_NoTitleBar;
		window_flags |= ImGuiWindowFlags_NoCollapse;
		window_flags |= ImGuiWindowFlags_NoResize;
		window_flags |= ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoSavedSettings;
		ImGui::Begin("##IBinaryWindows", nullptr, window_flags);

		float left_width = 320.0f;
		ImGui::BeginChild("Left", ImVec2(left_width, 0.0f), true);
		configPanel.OnRender();
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("Right", ImVec2(0.0f, 0.0f), true);
		if (ConfigPanel::GetConfig() == 0)
		{
			if (configPanel.GetFeatureIndex() == 0)
			{
				Bezier::RenderInImGuiChild();
			}
			else
			{
				RasterizerFeature::SetShaderOption(configPanel.GetRasterizerShaderIndex());
				RasterizerFeature::SetModelOption(configPanel.GetRasterizerModelIndex());
				RasterizerFeature::RenderInImGuiChild();
			}
		}
		else if (ConfigPanel::GetConfig() == 1)
		{
			OpenGLFeature::RenderInImGuiChild(configPanel.GetOpenGLModelIndex());
		}
		else
		{
			ImGui::Text("Content");
		}

		std::string capture_path;
		if (configPanel.ConsumeCaptureRequest(capture_path))
		{
			if (ConfigPanel::GetConfig() == 0)
			{
				bool ok = false;
				if (configPanel.GetFeatureIndex() == 0)
					ok = Bezier::SaveCurrentEditorPng(capture_path);
				else
					ok = RasterizerFeature::SaveCurrentPng(capture_path);

				if (ok)
				{
					const std::string filename = std::filesystem::path(capture_path).filename().string();
					NotificationCenter::Push("\xE7\x94\x9F\xE6\x88\x90\xE7\x9A\x84\xE5\x9B\xBE\xE7\x89\x87 " + filename, 3.0f);
				}
			}
		}

		ImGui::EndChild();
		ImGui::End();

		NotificationCenter::Render();

		ImGui::Render();

		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

		glfwMakeContextCurrent(window);
		glfwSwapBuffers(window);
	}

	RasterizerFeature::Shutdown();

	ImGui_ImplOpenGL2_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
