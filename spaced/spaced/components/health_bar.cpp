#include <levk/scene/entity.hpp>
#include <spaced/components/health_bar.hpp>
#include <algorithm>

namespace spaced {
void HealthBar::tick(levk::Duration) {
	n_value = std::clamp(n_value, 0.0f, 1.0f);
	auto quad_shape = std::make_unique<levk::QuadShape>();
	quad_shape->quad.size = {n_value * size.x, size.y};
	quad_shape->material_uri = "materials/unlit";
	auto left = glm::vec2{};
	left.x -= 0.5f * size.x;
	quad_shape->quad.origin = {left.x + 0.5f * quad_shape->quad.size.x, y_offset, z_index};
	quad_shape->quad.rgb = tint;
	set_shape(std::move(quad_shape));
}
} // namespace spaced
