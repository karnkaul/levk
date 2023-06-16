#include <levk/input/range.hpp>
#include <cassert>

namespace levk::input {
float Range::operator()(WindowInput const& input, Duration const dt) {
	auto ret = float{};
	for (auto& axis : key_axes) { ret += axis.tick(input, dt); }
	if (!gamepad_axes.empty()) {
		assert(!gamepad_index || *gamepad_index < input.gamepads.size());
		auto const& gamepad = input.gamepads[gamepad_index.value_or(input.last_engaged_gamepad_index)];
		for (auto const axis : gamepad_axes) { ret += gamepad.axes.value(axis); }
	}
	return ret;
}
} // namespace levk::input
