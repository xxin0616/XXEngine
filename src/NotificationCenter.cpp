#include "NotificationCenter.h"

#include "Include.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace NotificationCenter
{
	struct NotificationItem
	{
		std::string message;
		std::chrono::steady_clock::time_point expire_at;
	};

	struct PersistentItem
	{
		std::string message;
		std::chrono::steady_clock::time_point set_at;
		bool clear_pending = false;
	};

	using Clock = std::chrono::steady_clock;

	static std::vector<NotificationItem> g_items;
	static std::unordered_map<std::string, PersistentItem> g_persistent_items;
	static std::mutex g_mutex;

	static std::string BuildDecoratedMessage(const std::string& message)
	{
		if (message.find("渲染中") != std::string::npos)
			return "[RUN] " + message;
		if (message.find("渲染完成") != std::string::npos || message.find("生成的图片") != std::string::npos)
			return "[OK] " + message;
		return "[INFO] " + message;
	}

	void Push(const std::string& message, float duration_seconds)
	{
		const float duration = std::max(0.1f, duration_seconds);
		const auto expire = Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(duration));
		std::lock_guard<std::mutex> lock(g_mutex);
		g_items.push_back(NotificationItem{ message, expire });
	}

	void SetPersistent(const std::string& key, const std::string& message)
	{
		const auto now = Clock::now();
		std::lock_guard<std::mutex> lock(g_mutex);
		g_persistent_items[key] = PersistentItem{ message, now, false };
	}

	void ClearPersistent(const std::string& key)
	{
		const auto now = Clock::now();
		std::lock_guard<std::mutex> lock(g_mutex);
		auto it = g_persistent_items.find(key);
		if (it == g_persistent_items.end())
			return;
		it->second.clear_pending = true;
	}

	void Render()
	{
		std::vector<NotificationItem> items;
		std::vector<std::string> persistent_messages;
		{
			const auto now = Clock::now();
			const auto min_persistent_lifetime = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(0.25f));
			std::lock_guard<std::mutex> lock(g_mutex);
			g_items.erase(
				std::remove_if(
					g_items.begin(),
					g_items.end(),
					[now](const NotificationItem& item) { return item.expire_at <= now; }),
				g_items.end());
			items = g_items;

			for (auto it = g_persistent_items.begin(); it != g_persistent_items.end();)
			{
				if (it->second.clear_pending && now >= (it->second.set_at + min_persistent_lifetime))
					it = g_persistent_items.erase(it);
				else
					++it;
			}
			persistent_messages.reserve(g_persistent_items.size());
			for (const auto& kv : g_persistent_items)
				persistent_messages.push_back(kv.second.message);
		}

		if (items.empty() && persistent_messages.empty())
			return;

		ImGuiIO& io = ImGui::GetIO();
		const float margin_top = 16.0f;
		float y = margin_top;

		size_t persistent_index = 0;
		for (const std::string& msg : persistent_messages)
		{
			const std::string decorated = BuildDecoratedMessage(msg);
			const ImVec2 text_size = ImGui::CalcTextSize(decorated.c_str());
			const ImVec2 window_size = ImVec2(text_size.x + 56.0f, text_size.y + 30.0f);
			const float x = (io.DisplaySize.x - window_size.x) * 0.5f;

			ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
			ImGui::SetNextWindowSize(window_size, ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.97f);

			ImGuiWindowFlags flags = 0;
			flags |= ImGuiWindowFlags_NoDecoration;
			flags |= ImGuiWindowFlags_NoSavedSettings;
			flags |= ImGuiWindowFlags_NoMove;
			flags |= ImGuiWindowFlags_NoInputs;
			flags |= ImGuiWindowFlags_NoNav;
			flags |= ImGuiWindowFlags_NoFocusOnAppearing;

			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.28f, 0.55f, 0.97f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.65f, 0.82f, 1.0f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.98f, 0.99f, 1.0f, 1.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 12.0f));

			std::string window_name = "##NotificationPersistent_" + std::to_string(persistent_index++);
			if (ImGui::Begin(window_name.c_str(), nullptr, flags))
			{
				ImGui::SetWindowFontScale(1.08f);
				ImGui::TextUnformatted(decorated.c_str());
				ImGui::SetWindowFontScale(1.0f);
			}
			ImGui::End();
			ImGui::PopStyleVar(3);
			ImGui::PopStyleColor(3);

			y += window_size.y + 8.0f;
		}

		size_t item_index = 0;
		for (const NotificationItem& item : items)
		{
			const std::string decorated = BuildDecoratedMessage(item.message);
			const ImVec2 text_size = ImGui::CalcTextSize(decorated.c_str());
			const ImVec2 window_size = ImVec2(text_size.x + 56.0f, text_size.y + 30.0f);
			const float x = (io.DisplaySize.x - window_size.x) * 0.5f;

			ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
			ImGui::SetNextWindowSize(window_size, ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.96f);

			ImGuiWindowFlags flags = 0;
			flags |= ImGuiWindowFlags_NoDecoration;
			flags |= ImGuiWindowFlags_NoSavedSettings;
			flags |= ImGuiWindowFlags_NoMove;
			flags |= ImGuiWindowFlags_NoInputs;
			flags |= ImGuiWindowFlags_NoNav;
			flags |= ImGuiWindowFlags_NoFocusOnAppearing;

			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.17f, 0.36f, 0.20f, 0.96f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.72f, 0.96f, 0.74f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.99f, 1.0f, 0.99f, 1.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 12.0f));

			std::string window_name = "##Notification_" + std::to_string(item_index++);
			if (ImGui::Begin(window_name.c_str(), nullptr, flags))
			{
				ImGui::SetWindowFontScale(1.08f);
				ImGui::TextUnformatted(decorated.c_str());
				ImGui::SetWindowFontScale(1.0f);
			}
			ImGui::End();
			ImGui::PopStyleVar(3);
			ImGui::PopStyleColor(3);

			y += window_size.y + 8.0f;
		}
	}
}
