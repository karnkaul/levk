#include <levk/input/trigger.hpp>
#include <algorithm>

namespace levk::input {
namespace {
bool from_action(WindowInput const& input, Key const key, Action const action) {
	switch (action) {
	case Action::ePress: return input.keyboard.is_pressed(key); break;
	case Action::eHold: return input.keyboard.is_held(key); break;
	case Action::eRelease: return input.keyboard.is_released(key); break;
	default: return false;
	}
}
} // namespace

bool Trigger::operator()(WindowInput const& input) const {
	return std::ranges::any_of(keys, [&input, action = action](Key const key) { return from_action(input, key, action); });
}
} // namespace levk::input
