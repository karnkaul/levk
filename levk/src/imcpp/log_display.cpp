#include <imgui.h>
#include <imgui_internal.h>
#include <levk/imcpp/log_display.hpp>
#include <levk/util/fixed_string.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/reversed.hpp>
#include <algorithm>

namespace levk::imcpp {
namespace {
using Level = Logger::Level;

constexpr ImVec4 colour_for(Level const level) {
	switch (level) {
	case Level::eError: return {0.7f, 0.0f, 0.0f, 1.0f};
	case Level::eWarn: return {0.7f, 0.7f, 0.0f, 1.0f};
	case Level::eInfo: return {0.7f, 0.7f, 0.7f, 1.0f};
	default:
	case Level::eDebug: return {0.5f, 0.5f, 0.5f, 1.0f};
	}
}
} // namespace

LogDisplay::LogDisplay() {
	for (auto& level : show_levels.t) { level = true; }
}

void LogDisplay::draw_to(NotClosed<Window> w) {
	ImGui::Checkbox("Error", &show_levels[Level::eError]);
	ImGui::SameLine();
	ImGui::Checkbox("Warn", &show_levels[Level::eWarn]);
	ImGui::SameLine();
	ImGui::Checkbox("Info", &show_levels[Level::eInfo]);
	ImGui::SameLine();
	ImGui::Checkbox("Debug", &show_levels[Level::eDebug]);

	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();
	ImGui::Checkbox("Auto-scroll", &auto_scroll);

	if (ImGui::ArrowButton("##log_decrement", ImGuiDir_Left)) { display_count = std::clamp(display_count - 10, 10, 1000); }
	ImGui::SameLine();
	if (ImGui::ArrowButton("##log_increment", ImGuiDir_Right)) { display_count = std::clamp(display_count + 10, 10, 1000); }
	ImGui::SameLine();
	ImGui::Text("%s", FixedString{"Max: {}", display_count}.c_str());
	ImGui::SetNextItemWidth(200.0f);
	ImGui::SameLine();
	filter("Filter");
	ImGui::SameLine();
	if (ImGui::SmallButton("clear")) { filter = {}; }

	ImGui::Separator();
	int flags = ImGuiWindowFlags_HorizontalScrollbar;
	if (auto child = Window{w, "Entries", {}, {}, flags}) {
		auto display_list = std::vector<Logger::Entry>{};
		display_list.reserve(static_cast<std::size_t>(display_count));
		for (auto const& entry : entries_stack) {
			if (!show_levels[entry.level]) { continue; }
			if (!filter.empty() && entry.formatted_message.find(filter) == std::string::npos) { continue; }
			display_list.push_back(entry);
			if (static_cast<int>(display_list.size()) >= display_count) { break; }
		}
		for (auto const& entry : reversed(display_list)) {
			ImGui::TextColored(colour_for(entry.level), "%s", entry.formatted_message.c_str());
			if (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) { ImGui::SetScrollHereY(1.0f); }
		}
	}
}

void LogDisplay::init(std::span<Logger::Entry const> in) {
	for (auto const& entry : in) { on_log(entry); }
}

void LogDisplay::on_log(Logger::Entry const& entry) {
	entries_stack.push_front(entry);
	while (entries_stack.size() > max_entries_v) { entries_stack.pop_back(); }
}
} // namespace levk::imcpp
