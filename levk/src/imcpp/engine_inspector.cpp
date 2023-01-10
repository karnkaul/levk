#include <imgui.h>
#include <levk/imcpp/engine_inspector.hpp>
#include <levk/util/fixed_string.hpp>

namespace levk::imcpp {
void EngineInspector::inspect(NotClosed<Window>, Engine& engine, Time dt) {
	auto const ms = dt.count() * 1000.0f;
	m_dts.push(ms);
	float scale = engine.device().info().render_scale;
	if (ImGui::DragFloat("Render Scale", &scale, 0.05f, render_scale_limit_v[0], render_scale_limit_v[1])) { engine.device().set_render_scale(scale); }
	ImGui::Separator();
	ImGui::Text("%s", FixedString{"FPS: {}", engine.framerate()}.c_str());
	if (auto tn = TreeNode{"DeltaTime"}) {
		auto capacity = static_cast<int>(m_dts.capacity());
		if (ImGui::SliderInt("Samples", &capacity, 50, 500)) { m_dts.set_capacity(static_cast<std::size_t>(capacity)); }
		auto const dts = m_dts.span();
		ImGui::PlotLines("dt", dts.data(), static_cast<int>(dts.size()), 0, FixedString{"{:.2f}ms", ms}.c_str(), FLT_MAX, FLT_MAX, {0.0f, 50.0f});
	}
	ImGui::Separator();
	auto const render_stats = engine.device().stats();
	ImGui::Text("%s", FixedString{"Draw calls: {}", render_stats.draw_calls}.c_str());
	ImGui::Text("%s", FixedString{"Triangles: {}", render_stats.triangles}.c_str());
}
} // namespace levk::imcpp
