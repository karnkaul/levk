#pragma once
#include <levk/input/range.hpp>
#include <levk/input/trigger.hpp>

namespace spaced {
struct ControlScheme {
	levk::input::Range move_x{};
	levk::input::Range move_y{};
	levk::input::Trigger fire_turret{};
	levk::input::Trigger fire_cannon{};

	static ControlScheme make_default();
};
} // namespace spaced
