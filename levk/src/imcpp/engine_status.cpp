#include <imgui.h>
#include <imgui_internal.h>
#include <levk/imcpp/common.hpp>
#include <levk/imcpp/engine_status.hpp>
#include <levk/util/fixed_string.hpp>

namespace levk::imcpp {
namespace {
constexpr std::string_view to_str(Vsync const vsync) {
	switch (vsync) {
	case Vsync::eAdaptive: return "Adaptive";
	case Vsync::eMailbox: return "Mailbox";
	case Vsync::eOff: return "Off";
	case Vsync::eOn: return "On";
	default: return "Unknown";
	}
}

void vsync_combo(RenderDevice& device, RenderDeviceInfo const& device_info) {
	auto const supported = device_info.supported_vsync;
	if (auto combo = imcpp::Combo{"Vsync", to_str(device_info.current_vsync).data()}) {
		auto add = [&](Vsync const vsync) {
			if (supported.test(vsync)) {
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

void EngineStatus::draw_to(NotClosed<Window>, Engine& engine, Time dt) {
	auto const ms = dt.count() * 1000.0f;
	auto& device = engine.render_device();
	auto const& device_info = device.info();
	m_dts.resize(static_cast<std::size_t>(m_capacity));
	m_dts[m_offset] = ms;
	m_offset = (m_offset + 1) % static_cast<std::size_t>(m_capacity);
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
		ImGui::SliderInt("Samples", &m_capacity, 50, 500);
		ImGui::PlotLines("dt", m_dts.data(), static_cast<int>(m_dts.size()), static_cast<int>(m_offset), FixedString{"{:.2f}ms", ms}.c_str(), FLT_MAX, FLT_MAX,
						 {0.0f, 50.0f});
	}
	ImGui::Separator();
	ImGui::Text("%s", FixedString{"Draw calls: {}", device.draw_calls_last_frame()}.c_str());

	ImGui::Separator();
	if (auto tn = TreeNode{"Frame Profile"}) {
		auto const frame_profile = engine.frame_profile();
		auto const frame_time = frame_profile.profile[FrameProfile::Type::eFrameTime];
		ImGui::Text("%s", FixedString{"Frame time: {:.2f}ms", frame_time.count() * 1000.0f}.c_str());
		for (FrameProfile::Type type = FrameProfile::Type{}; type < FrameProfile::Type::eFrameTime; type = FrameProfile::Type(int(type) + 1)) {
			auto const ratio = frame_profile.profile[type] / frame_time;
			auto const label = FrameProfile::to_string_v[type];
			auto const overlay = FixedString{"{} ({:.0f}%)", label, ratio * 100.0f};
			ImGui::ProgressBar(ratio, {-1.0f, 0.0f}, overlay.c_str());
		}
	}
}
} // namespace levk::imcpp
