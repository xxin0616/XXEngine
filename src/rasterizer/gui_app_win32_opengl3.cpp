#include "rasterizer.hpp"
#include "Triangle.hpp"

#include "imgui.h"
#include "imgui_impl_opengl2.h"
#include "imgui_impl_win32.h"

#include <Eigen/Eigen>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>

#include <opencv2/opencv.hpp>
#include <algorithm>
#include <iostream>

Eigen::Matrix4f get_view_matrix(Eigen::Vector3f eye_pos);
Eigen::Matrix4f get_projection_matrix(float eye_fov, float zNear, float zFar);
Eigen::Matrix4f get_model_matrix(float angle, float scaleSize);

struct WGL_WindowData { HDC hDC; };

static HGLRC g_hRC = nullptr;
static WGL_WindowData g_MainWindow = {};
static int g_Width = 1600;
static int g_Height = 900;

static bool CreateDeviceWGL(HWND hWnd, WGL_WindowData* data)
{
	HDC hDc = ::GetDC(hWnd);
	PIXELFORMATDESCRIPTOR pfd = {};
	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;

	const int pf = ::ChoosePixelFormat(hDc, &pfd);
	if (pf == 0)
		return false;
	if (::SetPixelFormat(hDc, pf, &pfd) == FALSE)
		return false;
	::ReleaseDC(hWnd, hDc);

	data->hDC = ::GetDC(hWnd);
	if (!g_hRC)
		g_hRC = wglCreateContext(data->hDC);
	return true;
}

static void CleanupDeviceWGL(HWND hWnd, WGL_WindowData* data)
{
	wglMakeCurrent(nullptr, nullptr);
	::ReleaseDC(hWnd, data->hDC);
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED)
		{
			g_Width = LOWORD(lParam);
			g_Height = HIWORD(lParam);
		}
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU)
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

static GLuint create_texture_rgb(int w, int h)
{
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);
	return tex;
}

static void update_texture_rgb(GLuint tex, int w, int h, const unsigned char* data)
{
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, data);
	glBindTexture(GL_TEXTURE_2D, 0);
}

static GLuint load_texture_rgb(const std::string& file)
{
	cv::Mat img = cv::imread(file, cv::IMREAD_COLOR);
	if (img.empty()) return 0;
	cv::cvtColor(img, img, cv::COLOR_BGR2RGB);
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img.cols, img.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, img.data);
	glBindTexture(GL_TEXTURE_2D, 0);
	return tex;
}

static void draw_textured_quad(GLuint tex)
{
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tex);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1, 0, 1, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glBegin(GL_QUADS);
	glTexCoord2f(0, 1); glVertex2f(0, 0);
	glTexCoord2f(1, 1); glVertex2f(1, 0);
	glTexCoord2f(1, 0); glVertex2f(1, 1);
	glTexCoord2f(0, 0); glVertex2f(0, 1);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);
}

static void render_gpu_fixed(const std::vector<Triangle*>& triangles, GLuint tex, const Eigen::Matrix4f& proj, const Eigen::Matrix4f& view, const Eigen::Matrix4f& model)
{
	glEnable(GL_DEPTH_TEST);
	glClear(GL_DEPTH_BUFFER_BIT);

	Eigen::Matrix4f mv = view * model;
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(proj.data());
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(mv.data());

	if (tex != 0)
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, tex);
	}

	glBegin(GL_TRIANGLES);
	for (auto* t : triangles)
	{
		for (int i = 0; i < 3; i++)
		{
			auto n = t->normal[i].normalized();
			glNormal3f(n.x(), n.y(), n.z());
			auto uv = t->tex_coords[i];
			glTexCoord2f(uv.x(), 1.0f - uv.y());
			auto p = t->v[i];
			glVertex3f(p.x(), p.y(), p.z());
		}
	}
	glEnd();

	if (tex != 0)
	{
		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_TEXTURE_2D);
	}
	glDisable(GL_DEPTH_TEST);
}

int run_gui_app(rst::rasterizer& sw, std::vector<Triangle*>& triangles, const std::string& texture_file, float& angle, float& scale, Eigen::Vector3f& eye_pos)
{
	ImGui_ImplWin32_EnableDpiAwareness();
	const float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

	WNDCLASSEXW wc = { sizeof(wc), CS_OWNDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"RasterizerApp", nullptr };
	::RegisterClassExW(&wc);
	HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Rasterizer", WS_OVERLAPPEDWINDOW, 100, 100, (int)(g_Width * main_scale), (int)(g_Height * main_scale), nullptr, nullptr, wc.hInstance, nullptr);

	if (!CreateDeviceWGL(hwnd, &g_MainWindow))
	{
		CleanupDeviceWGL(hwnd, &g_MainWindow);
		::DestroyWindow(hwnd);
		::UnregisterClassW(wc.lpszClassName, wc.hInstance);
		return 1;
	}
	wglMakeCurrent(g_MainWindow.hDC, g_hRC);

	::ShowWindow(hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(hwnd);

	RECT rc;
	::GetClientRect(hwnd, &rc);
	g_Width = rc.right - rc.left;
	g_Height = rc.bottom - rc.top;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(main_scale);
	style.FontScaleDpi = main_scale;

	{
		const ImWchar* ranges = io.Fonts->GetGlyphRangesChineseFull();
		ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 18.0f * main_scale, nullptr, ranges);
		(void)font;
	}

	ImGui_ImplWin32_InitForOpenGL(hwnd);
	ImGui_ImplOpenGL2_Init();

	const int sw_w = 700;
	const int sw_h = 700;
	GLuint sw_tex = create_texture_rgb(sw_w, sw_h);
	GLuint placeholder_tex = load_texture_rgb("models/DamagedHelmet/screenshot/screenshot.png");
	GLuint gpu_tex = load_texture_rgb(texture_file);

	bool use_gpu = false;
	bool needs_render = true;
	const float mouse_sensitivity = 0.01f;
	const int max_pixel_delta = 50;

	ImVec2 viewport_min = ImVec2(0, 0);
	ImVec2 viewport_max = ImVec2(0, 0);

	bool done = false;
	while (!done)
	{
		MSG msg;
		while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				done = true;
		}
		if (done)
			break;

		ImGui_ImplOpenGL2_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		const float panel_w = std::clamp(static_cast<float>(g_Width) * 0.28f, 360.0f, 520.0f);
		const float panel_h = static_cast<float>(g_Height);
		const float view_w = std::max(1.0f, static_cast<float>(g_Width) - panel_w);

		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImVec2(panel_w, panel_h));
		ImGui::Begin("閰嶇疆", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

		int backend_idx = use_gpu ? 1 : 0;
		ImGui::Text("鍚庣");
		if (ImGui::RadioButton("杞欢鍏夋爡鍖栧櫒", backend_idx == 0)) {
			backend_idx = 0;
			use_gpu = false;
			needs_render = true;
		}
		if (ImGui::RadioButton("鏅€?OpenGL", backend_idx == 1)) {
			backend_idx = 1;
			use_gpu = true;
			needs_render = true;
		}

		ImGui::Separator();

		float a = angle;
		if (ImGui::SliderFloat("Angle", &a, -360.0f, 360.0f))
		{
			angle = a;
			needs_render = true;
		}
		float sc = scale;
		if (ImGui::SliderFloat("Scale", &sc, 0.1f, 10.0f))
		{
			scale = sc;
			needs_render = true;
		}
		if (ImGui::Button("Render"))
		{
			needs_render = true;
		}
		ImGui::Separator();
		ImGui::Text("A/D rotate, W/S scale");
		ImGui::Text("Drag in viewport to move camera");
		ImGui::Text("ESC exit");
		ImGui::End();

		ImGui::SetNextWindowPos(ImVec2(panel_w, 0));
		ImGui::SetNextWindowSize(ImVec2(view_w, panel_h));
		ImGui::Begin("瑙嗗彛", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground);
		ImVec2 avail = ImGui::GetContentRegionAvail();
		if (avail.x < 1) avail.x = 1;
		if (avail.y < 1) avail.y = 1;
		ImGui::InvisibleButton("ViewportArea", avail, ImGuiButtonFlags_MouseButtonLeft);
		viewport_min = ImGui::GetItemRectMin();
		viewport_max = ImGui::GetItemRectMax();

		const bool hovered = ImGui::IsItemHovered();
		if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
			ImVec2 d = io.MouseDelta;
			const int dx = static_cast<int>(d.x);
			const int dy = static_cast<int>(d.y);
			if (std::abs(dx) <= max_pixel_delta && std::abs(dy) <= max_pixel_delta)
			{
				eye_pos.x() += static_cast<float>(dx) * mouse_sensitivity;
				eye_pos.y() -= static_cast<float>(dy) * mouse_sensitivity;
				eye_pos.y() = std::clamp(eye_pos.y(), -10.0f, 10.0f);
				needs_render = true;
				std::cout << "mouse dx=" << dx << " dy=" << dy << " eye=(" << eye_pos.x() << "," << eye_pos.y() << "," << eye_pos.z() << ")\n";
			}
			else
			{
				std::cout << "mouse skip dx=" << dx << " dy=" << dy << " eye=(" << eye_pos.x() << "," << eye_pos.y() << "," << eye_pos.z() << ")\n";
			}
		}

		if (ImGui::IsKeyPressed(ImGuiKey_A))
		{
			angle += 10.0f;
			needs_render = true;
			std::cout << "key=a angle=" << angle << " scale=" << scale << "\n";
		}
		if (ImGui::IsKeyPressed(ImGuiKey_D))
		{
			angle -= 10.0f;
			needs_render = true;
			std::cout << "key=d angle=" << angle << " scale=" << scale << "\n";
		}
		if (ImGui::IsKeyPressed(ImGuiKey_W))
		{
			scale += 0.1f;
			needs_render = true;
			std::cout << "key=w angle=" << angle << " scale=" << scale << "\n";
		}
		if (ImGui::IsKeyPressed(ImGuiKey_S))
		{
			scale -= 0.1f;
			if (scale < 0.1f) scale = 0.1f;
			needs_render = true;
			std::cout << "key=s angle=" << angle << " scale=" << scale << "\n";
		}
		if (ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			std::cout << "key=ESC exit\n";
			done = true;
		}

		ImGui::End();

		ImGui::Render();

		glViewport(0, 0, g_Width, g_Height);
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		int vx = static_cast<int>(viewport_min.x);
		int vy_top = static_cast<int>(viewport_min.y);
		int vw = static_cast<int>(viewport_max.x - viewport_min.x);
		int vh = static_cast<int>(viewport_max.y - viewport_min.y);
		if (vw < 1) vw = 1;
		if (vh < 1) vh = 1;
		int vy = g_Height - (vy_top + vh);
		if (vy < 0) vy = 0;

		glEnable(GL_SCISSOR_TEST);
		glScissor(vx, vy, vw, vh);
		glViewport(vx, vy, vw, vh);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (!use_gpu)
		{
			(void)sw;
			(void)triangles;
			(void)sw_tex;
			needs_render = false;
			if (placeholder_tex != 0)
			{
				draw_textured_quad(placeholder_tex);
			}
		}
		else
		{
			Eigen::Matrix4f proj = get_projection_matrix(45, 0.1, 50);
			Eigen::Matrix4f view = get_view_matrix(eye_pos);
			Eigen::Matrix4f model = get_model_matrix(angle, scale);
			render_gpu_fixed(triangles, gpu_tex, proj, view, model);
		}

		glDisable(GL_SCISSOR_TEST);
		glViewport(0, 0, g_Width, g_Height);
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
		::SwapBuffers(g_MainWindow.hDC);
	}

	if (sw_tex) glDeleteTextures(1, &sw_tex);
	if (placeholder_tex) glDeleteTextures(1, &placeholder_tex);
	if (gpu_tex) glDeleteTextures(1, &gpu_tex);

	ImGui_ImplOpenGL2_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CleanupDeviceWGL(hwnd, &g_MainWindow);
	wglDeleteContext(g_hRC);
	::DestroyWindow(hwnd);
	::UnregisterClassW(wc.lpszClassName, wc.hInstance);

	return 0;
}



