#pragma once
#include <glm/vec2.hpp>
#include <levk/util/flex_array.hpp>
#include <levk/window/common.hpp>
#include <levk/window/gamepad.hpp>
#include <array>
#include <optional>

namespace levk {
struct WindowInput {
	ActionState<Key, 512> keyboard{};
	ActionState<MouseButton, 512> mouse{};
	std::array<Gamepad, 16> gamepads{};
	std::size_t last_engaged_gamepad_index{};
	FlexArray<std::uint32_t, 8> codepoints{};
	glm::vec2 cursor{};
	glm::vec2 scroll{};
	glm::vec2 ui_space{};
};
} // namespace levk
