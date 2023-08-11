#include <imgui.h>
#include <levk/scene/entity.hpp>
#include <levk/scene/shape_renderer.hpp>
#include <spaced/components/cannon.hpp>

namespace spaced {
bool Cannon::fire_beam() {
	if (is_firing()) { return false; }
	m_remain = duration;
	return true;
}

void Cannon::setup() { owning_entity()->attach(std::make_unique<levk::ShapeRenderer>()); }

void Cannon::tick(levk::Duration dt) {
	if (m_remain < 0s) { return; }

	auto& renderer = owning_entity()->get_component<levk::ShapeRenderer>();
	auto quad = std::make_unique<levk::QuadShape>();

	m_remain -= dt;
	if (m_remain < 0s) {
		m_remain = {};
		quad->quad.size = {};
		renderer.set_shape(std::move(quad));
		return;
	}

	auto const& self = owning_entity()->transform().position();
	quad->quad.size = {target.x - self.x, height};
	quad->quad.origin.x = 0.5f * quad->quad.size.x;
	quad->quad.rgb = tint;
	quad->material_uri = "materials/unlit";
	renderer.set_shape(std::move(quad));
}

void Cannon::inspect(levk::imcpp::OpenWindow) {
	auto dur = duration.count();
	if (ImGui::DragFloat("Duration", &dur, 0.25f, 0.25f, 10.0f)) { duration = levk::Duration{dur}; }
	ImGui::DragFloat("DPS", &dps, 1.0f, 1.0f, 1000.0f);
	ImGui::DragFloat("Beam Height", &height, 0.01f, 0.01f, 1.0f);
}
} // namespace spaced
