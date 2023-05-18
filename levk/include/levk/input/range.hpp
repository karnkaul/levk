#pragma once
#include <levk/input/key_axis.hpp>
#include <levk/window/gamepad.hpp>
#include <optional>
#include <vector>

namespace levk::input {
struct Range {
	std::vector<levk::KeyAxis> key_axes{};
	std::vector<levk::GamepadAxis> gamepad_axes{};
	std::optional<std::size_t> gamepad_index{};

	float operator()(levk::WindowInput const& input, levk::Duration dt);
};
} // namespace levk::input
