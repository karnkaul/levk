#pragma once
#include <glm/vec2.hpp>
#include <levk/util/flex_array.hpp>
#include <levk/window/action_state.hpp>
#include <levk/window/common.hpp>
#include <array>

namespace levk {
struct WindowInput {
	ActionState<Key, 512> keyboard{};
	ActionState<MouseButton, 512> mouse{};
	FlexArray<std::uint32_t, 8> codepoints{};
	glm::vec2 cursor{};
	glm::vec2 scroll{};
	glm::vec2 ui_space{};
};
} // namespace levk
