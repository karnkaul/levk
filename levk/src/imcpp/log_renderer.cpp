#include <imgui.h>
#include <imgui_internal.h>
#include <levk/imcpp/log_renderer.hpp>
#include <levk/util/fixed_string.hpp>
#include <levk/util/logger.hpp>
#include <algorithm>

namespace levk::imcpp {
namespace {
struct Access : logger::Accessor {
	LogRenderer& out;

	Access(LogRenderer& out) : out(out) {}

	static constexpr ImVec4 colour_for(logger::Level const level) {
		switch (level) {
		case logger::Level::eError: return {0.7f, 0.0f, 0.0f, 1.0f};
		case logger::Level::eWarn: return {0.7f, 0.7f, 0.0f, 1.0f};
		case logger::Level::eInfo: return {0.7f, 0.7f, 0.7f, 1.0f};
		default:
		case logger::Level::eDebug: return {0.5f, 0.5f, 0.5f, 1.0f};
		}
	}

	void operator()(std::span<logger::Entry const> entries) override {
		if (auto style = StyleVar{ImGuiStyleVar_ItemSpacing, glm::vec2{}}) {
			out.cache.clear();
			for (auto const& entry : entries) {
				if (!out.show_levels[entry.level]) { continue; }
				if (!out.filter.empty() && entry.message.find(out.filter) == std::string::npos) { continue; }
				out.cache.push(entry);
			}
			for (auto const& entry : out.cache.span()) { ImGui::TextColored(colour_for(entry.level), "%s", entry.message.c_str()); }
			if (out.auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) { ImGui::SetScrollHereY(1.0f); }
		}
	}
};
} // namespace

LogRenderer::LogRenderer() noexcept {
	for (auto& level : show_levels.t) { level = true; }
}

void LogRenderer::display(NotClosed<Window> w) {
	ImGui::Checkbox("Error", &show_levels[logger::Level::eError]);
	ImGui::SameLine();
	ImGui::Checkbox("Warn", &show_levels[logger::Level::eWarn]);
	ImGui::SameLine();
	ImGui::Checkbox("Info", &show_levels[logger::Level::eInfo]);
	ImGui::SameLine();
	ImGui::Checkbox("Debug", &show_levels[logger::Level::eDebug]);

	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();
	ImGui::Checkbox("Auto-scroll", &auto_scroll);

	if (ImGui::ArrowButton("##log_decrement", ImGuiDir_Left)) { entries = std::clamp(entries - 10, 10, 1000); }
	ImGui::SameLine();
	if (ImGui::ArrowButton("##log_increment", ImGuiDir_Right)) { entries = std::clamp(entries + 10, 10, 1000); }
	ImGui::SameLine();
	ImGui::Text("%s", FixedString{"Max: {}", entries}.c_str());
	cache.set_capacity(static_cast<std::size_t>(entries));
	ImGui::SetNextItemWidth(200.0f);
	ImGui::SameLine();
	filter("Filter");
	ImGui::SameLine();
	if (ImGui::SmallButton("clear")) { filter = {}; }

	ImGui::Separator();
	int flags = ImGuiWindowFlags_HorizontalScrollbar;
	if (auto child = Window{w, "Entries", {}, {}, flags}) {
		auto access_log = Access{*this};
		logger::access_buffer(access_log);
	}
}
} // namespace levk::imcpp
