#include <imgui.h>
#include <levk/imcpp/reflector.hpp>
#include <levk/scene/entity.hpp>
#include <levk/scene/scene.hpp>
#include <levk/util/logger.hpp>
#include <levk/window/window_state.hpp>
#include <spaced/components/player_controller.hpp>
#include <algorithm>

namespace spaced {
void PlayerController::inspect(levk::imcpp::OpenWindow w) {
	ImGui::DragFloat("Y Speed", &y_speed, 0.25f, 0.1f, 50.0f);
	auto const reflector = levk::imcpp::Reflector{w};
	if (auto tn = levk::imcpp::TreeNode{"Bounds"}) {
		reflector("lo", bounds.lo);
		reflector("hi", bounds.hi);
	}
}

void PlayerController::tick(levk::Duration dt) {
	auto const& input = window_state().input;

	auto const y_in = control_scheme.move_y(input, dt);
	auto const dy = y_in * y_speed * dt.count();
	auto& transform = owning_scene()->get_entity(move_target).transform();
	auto const new_position = glm::clamp(transform.position() + glm::vec3{0.0f, dy, 0.0f}, bounds.lo, bounds.hi);
	transform.set_position(new_position);

	m_roll = -y_in * roll_limit.value;
	owning_scene()->get_entity(rotate_target).transform().set_orientation(glm::angleAxis(m_roll.to_radians().value, levk::right_v));
}
} // namespace spaced
