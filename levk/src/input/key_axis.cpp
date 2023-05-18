#include <levk/input/key_axis.hpp>

namespace levk {
float KeyAxis::tick(WindowInput const& window_input, Duration const dt) {
	static constexpr auto unit = [](float const in) { return in > 0.0f ? 1.0f : -1.0f; };
	auto raw_input = float{};
	if (window_input.keyboard.is_held(lo)) { raw_input -= 1.0f; }
	if (window_input.keyboard.is_held(hi)) { raw_input += 1.0f; }

	if (raw_input == 0.0f && std::abs(value) < 0.1f) {
		value = 0.0f;
	} else {
		auto const dv = raw_input == 0.0f ? -unit(value) : raw_input;
		value += dv * dt / lerp_rate;
	}
	value = std::clamp(value, -1.0f, 1.0f);
	return value;
}
} // namespace levk
