#include <spaced/game/control_scheme.hpp>

namespace spaced {
ControlScheme ControlScheme::make_default() {
	auto ret = ControlScheme{};
	ret.move_x.key_axes = {
		{levk::Key::eA, levk::Key::eD},
		{levk::Key::eLeft, levk::Key::eRight},
	};
	ret.move_x.gamepad_axes = {levk::GamepadAxis::eLeftX};

	ret.move_y.key_axes = {
		{levk::Key::eS, levk::Key::eW},
		{levk::Key::eDown, levk::Key::eUp},
	};
	ret.move_y.gamepad_axes = {levk::GamepadAxis::eLeftY};

	ret.fire_turret.keys = {levk::Key::eSpace};
	ret.fire_turret.actions = {levk::Action::ePress, levk::Action::eHold};

	ret.fire_cannon.keys = {levk::Key::eEnter};
	ret.fire_cannon.actions = {levk::Action::ePress, levk::Action::eHold};
	return ret;
}
} // namespace spaced
