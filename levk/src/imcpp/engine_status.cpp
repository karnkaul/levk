#include <imgui.h>
#include <imgui_internal.h>
#include <levk/imcpp/common.hpp>
#include <levk/imcpp/engine_status.hpp>
#include <levk/util/fixed_string.hpp>

namespace levk::imcpp {
namespace {
constexpr std::string_view to_str(Vsync::Type const vsync) {
	switch (vsync) {
	case Vsync::eAdaptive: return "Adaptive";
	case Vsync::eMailbox: return "Mailbox";
	case Vsync::eOff: return "Off";
	case Vsync::eOn: return "On";
	default: return "Unknown";
	}
}

void vsync_combo(RenderDevice& device, GraphicsDeviceInfo const& device_info) {
	auto const supported = device_info.supported_vsync;
	if (auto combo = imcpp::Combo{"Vsync", to_str(device_info.current_vsync).data()}) {
		auto add = [&](Vsync::Type const vsync) {
			if (supported.flags & vsync) {
				if (ImGui::Selectable(to_str(vsync).data(), device_info.current_vsync == vsync)) { device.set_vsync(vsync); }
			}
		};
		add(Vsync::eAdaptive);
		add(Vsync::eMailbox);
		add(Vsync::eOn);
		add(Vsync::eOff);
	}
}
} // namespace

void EngineStatus::draw_to(NotClosed<Window>, Engine& engine) {
	auto const ms = engine.delta_time().count() * 1000.0f;
	auto& device = engine.render_device();
	auto const& device_info = device.info();
	m_dts.push(ms);
	ImGui::Text("%s", FixedString{"Device: {}{}", device_info.name, device_info.validation ? " [v]" : ""}.c_str());
	ImGui::Text("%s", FixedString{"MSAA: {}x", static_cast<int>(device_info.current_aa)}.c_str());
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();
	ImGui::Text("%s", FixedString{"Swapchain: {}", device_info.swapchain == ColourSpace::eLinear ? "Linear" : "sRGB"}.c_str());
	vsync_combo(device, device_info);
	float scale = device.info().render_scale;
	if (ImGui::DragFloat("Render Scale", &scale, 0.05f, render_scale_limit_v[0], render_scale_limit_v[1])) { device.set_render_scale(scale); }

	ImGui::Separator();
	ImGui::Text("%s", FixedString{"FPS: {}", engine.framerate()}.c_str());
	if (auto tn = TreeNode{"DeltaTime"}) {
		auto capacity = static_cast<int>(m_dts.capacity());
		if (ImGui::SliderInt("Samples", &capacity, 50, 500)) { m_dts.set_capacity(static_cast<std::size_t>(capacity)); }
		auto const dts = m_dts.span();
		ImGui::PlotLines("dt", dts.data(), static_cast<int>(dts.size()), 0, FixedString{"{:.2f}ms", ms}.c_str(), FLT_MAX, FLT_MAX, {0.0f, 50.0f});
	}
	ImGui::Separator();
	auto const render_stats = device.stats();
	ImGui::Text("%s", FixedString{"Draw calls: {}", render_stats.draw_calls}.c_str());
}
} // namespace levk::imcpp
