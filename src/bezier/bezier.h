#pragma once
#ifndef _BEZIER_H_
#define _BEZIER_H_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "../Include.h"
#include "../../lib/stb/stb_image_write.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace Bezier
{
	struct Vec2
	{
		float x;
		float y;
	};

	static std::vector<Vec2> control_points;
	static GLuint texture_id = 0;
	static int tex_w = 0;
	static int tex_h = 0;
	static std::vector<std::uint8_t> rgba;
	static constexpr int kMinBezierOrder = 2;
	static constexpr int kMaxBezierOrder = 8;
	static int bezier_order = 4;

	inline Vec2 recursive_bezier(const std::vector<Vec2>& points, float t)
	{
		if (points.size() == 1)
			return points[0];

		std::vector<Vec2> next;
		next.reserve(points.size() - 1);
		for (size_t i = 0; i + 1 < points.size(); i++)
		{
			next.push_back(Vec2{
				points[i].x * (1.0f - t) + points[i + 1].x * t,
				points[i].y * (1.0f - t) + points[i + 1].y * t
				});
		}
		return recursive_bezier(next, t);
	}

	inline void clear()
	{
		control_points.clear();
	}

	inline void ensure_texture(int w, int h)
	{
		if (w <= 0 || h <= 0)
			return;
		if (w == tex_w && h == tex_h && texture_id != 0)
			return;

		tex_w = w;
		tex_h = h;
		rgba.assign((size_t)tex_w * (size_t)tex_h * 4u, 0u);

		if (texture_id == 0)
			glGenTextures(1, &texture_id);

		glBindTexture(GL_TEXTURE_2D, texture_id);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_w, tex_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
	}

	inline void set_pixel(int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255u)
	{
		if (x < 0 || y < 0 || x >= tex_w || y >= tex_h)
			return;
		size_t idx = ((size_t)y * (size_t)tex_w + (size_t)x) * 4u;
		rgba[idx + 0] = r;
		rgba[idx + 1] = g;
		rgba[idx + 2] = b;
		rgba[idx + 3] = a;
	}

	inline void draw_circle(int cx, int cy, int radius, std::uint8_t r, std::uint8_t g, std::uint8_t b)
	{
		int rr = radius * radius;
		for (int y = cy - radius; y <= cy + radius; y++)
		{
			for (int x = cx - radius; x <= cx + radius; x++)
			{
				int dx = x - cx;
				int dy = y - cy;
				if (dx * dx + dy * dy <= rr)
					set_pixel(x, y, r, g, b);
			}
		}
	}

	inline void render_curve()
	{
		if (control_points.size() != (size_t)bezier_order + 1u)
			return;

		for (double t = 0.0; t <= 1.0; t += 0.001)
		{
			Vec2 p = recursive_bezier(control_points, (float)t);
			int x_int = (int)std::floor(p.x);
			int y_int = (int)std::floor(p.y);
			float x_frac = p.x - (float)x_int;
			float y_frac = p.y - (float)y_int;

			int x0 = x_frac < 0.5f ? x_int - 1 : x_int;
			int y0 = y_frac < 0.5f ? y_int - 1 : y_int;

			for (int i = 0; i <= 1; i++)
			{
				for (int j = 0; j <= 1; j++)
				{
					int px = x0 + i;
					int py = y0 + j;
					float dx = std::abs(p.x - (float)x0 - (float)i - 0.5f);
					float dy = std::abs(p.y - (float)y0 - (float)j - 0.5f);
					float dist2 = dx * dx + dy * dy;
					float weight = 1.0f - dist2 / 2.0f;
					if (weight <= 0.0f)
						continue;
					if (px < 0 || py < 0 || px >= tex_w || py >= tex_h)
						continue;

					size_t idx = ((size_t)py * (size_t)tex_w + (size_t)px) * 4u;
					float prev = (float)rgba[idx + 1];
					float val = std::max(255.0f * weight, prev);
					rgba[idx + 1] = (std::uint8_t)std::min(255.0f, val);
					rgba[idx + 3] = 255u;
				}
			}
		}
	}

	inline void upload()
	{
		if (texture_id == 0 || tex_w <= 0 || tex_h <= 0)
			return;
		glBindTexture(GL_TEXTURE_2D, texture_id);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex_w, tex_h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
	}

	inline bool SaveCurrentEditorPng(const std::string& outputPath)
	{
		if (tex_w <= 0 || tex_h <= 0 || rgba.empty())
			return false;

		std::vector<std::uint8_t> flipped(rgba.size(), 0u);
		const size_t row_bytes = (size_t)tex_w * 4u;
		for (int y = 0; y < tex_h; y++)
		{
			const size_t src = (size_t)y * row_bytes;
			const size_t dst = (size_t)(tex_h - 1 - y) * row_bytes;
			std::copy(rgba.begin() + src, rgba.begin() + src + row_bytes, flipped.begin() + dst);
		}

		return stbi_write_png(outputPath.c_str(), tex_w, tex_h, 4, flipped.data(), tex_w * 4) != 0;
	}

	inline void RenderInImGuiChild()
	{
		bezier_order = std::clamp(bezier_order, kMinBezierOrder, kMaxBezierOrder);

		ImVec2 total_avail = ImGui::GetContentRegionAvail();
		float editor_height = std::max(140.0f, std::floor(total_avail.y * (2.0f / 3.0f)));

		if (ImGui::BeginChild("##BezierEditor", ImVec2(0.0f, editor_height), true))
		{
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("贝塞尔曲线");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(90.0f);
			if (ImGui::BeginCombo("##BezierOrder", std::to_string(bezier_order).c_str()))
			{
				for (int order = kMinBezierOrder; order <= kMaxBezierOrder; order++)
				{
					const bool selected = (order == bezier_order);
					if (ImGui::Selectable(std::to_string(order).c_str(), selected))
					{
						bezier_order = order;
						clear();
					}
					if (selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear"))
				clear();

			ImVec2 avail = ImGui::GetContentRegionAvail();
			int w = (int)std::max(64.0f, std::floor(avail.x));
			int h = (int)std::max(64.0f, std::floor(avail.y));
			ensure_texture(w, h);

			std::fill(rgba.begin(), rgba.end(), 0u);
			for (const Vec2& p : control_points)
				draw_circle((int)std::round(p.x), (int)std::round(p.y), 4, 255u, 255u, 255u);
			render_curve();
			upload();

			ImVec2 image_size = ImVec2((float)tex_w, (float)tex_h);
			ImGui::Image((ImTextureID)(intptr_t)texture_id, image_size, ImVec2(0, 1), ImVec2(1, 0));

			if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				ImVec2 min = ImGui::GetItemRectMin();
				ImVec2 mouse = ImGui::GetIO().MousePos;
				ImVec2 rel = ImVec2(mouse.x - min.x, mouse.y - min.y);
				if (rel.x >= 0.0f && rel.y >= 0.0f && rel.x < image_size.x && rel.y < image_size.y)
				{
					if (control_points.size() < (size_t)bezier_order + 1u)
					{
						// ImGui::Image uses flipped V (uv0.y=1, uv1.y=0), so click Y must be flipped back.
						float px = std::clamp(rel.x, 0.0f, image_size.x - 1.0f);
						float py = std::clamp(image_size.y - 1.0f - rel.y, 0.0f, image_size.y - 1.0f);
						control_points.push_back(Vec2{ px, py });
					}
				}
			}
		}
		ImGui::EndChild();

		if (ImGui::BeginChild("##BezierIntroduction", ImVec2(0.0f, 0.0f), true))
		{
			ImGui::TextUnformatted("贝塞尔曲线详细介绍");
			ImGui::Separator();
			ImGui::TextWrapped("贝塞尔曲线是一种由控制点定义的参数曲线，广泛用于图形学、字体和动画路径。");
			ImGui::TextWrapped("当前为 %d 阶曲线，需要 %d 个控制点；在上方编辑器中按顺序点击即可完成输入。", bezier_order, bezier_order + 1);
			ImGui::Spacing();
			ImGui::TextWrapped("定义公式：B(t) = Σ C(n, i) * (1 - t)^(n - i) * t^i * P_i，t 的范围是 [0, 1]。");
			ImGui::TextWrapped("你现在看到的实现使用递归 de Casteljau 算法，数值稳定、实现直观，适合交互式编辑。");
			ImGui::Spacing();
			ImGui::TextWrapped("核心性质：");
			ImGui::BulletText("曲线一定经过起点 P0 与终点 Pn。");
			ImGui::BulletText("曲线始终位于控制点形成的凸包内部。");
			ImGui::BulletText("阶数越高，可表达形状越复杂，但调参难度也会提升。");
			ImGui::BulletText("拖动或重设控制点会直接改变曲线局部与整体形状。");
		}
		ImGui::EndChild();
	}

}

#endif



