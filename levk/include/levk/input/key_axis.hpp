#pragma once
#include <levk/util/time.hpp>
#include <levk/window/window_input.hpp>

namespace levk {
struct KeyAxis {
	Key lo{};
	Key hi{};
	Duration lerp_rate{0.2s};

	float value{};

	float tick(WindowInput const& window_input, Duration dt);
};
} // namespace levk
