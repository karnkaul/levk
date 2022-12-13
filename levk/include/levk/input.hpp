#pragma once
#include <glm/vec2.hpp>
#include <levk/util/flex_array.hpp>
#include <levk/window_common.hpp>
#include <array>

namespace levk {
struct Input {
	std::array<Action, 512> keyboard{};
	std::array<Action, 16> mouse{};
	FlexArray<std::uint32_t, 8> codepoints{};
	glm::vec2 cursor{};
	glm::vec2 scroll{};

	template <typename T>
	static constexpr Action action(std::span<Action const> actions, T const t) {
		auto const index = static_cast<std::size_t>(t);
		if (index >= actions.size()) { return Action::eNone; }
		return actions[index];
	}

	constexpr Action keyboard_action(Key const key) const { return action(keyboard, key); }
	constexpr Action mouse_action(MouseButton const button) const { return action(mouse, button); }

	constexpr bool is_pressed(Key const key) const { return keyboard_action(key) == Action::ePress; }
	constexpr bool is_held(Key const key) const { return keyboard_action(key) == Action::eHold; }
	constexpr bool is_repeated(Key const key) const { return keyboard_action(key) == Action::eRepeat; }
	constexpr bool is_released(Key const key) const { return keyboard_action(key) == Action::eRelease; }

	constexpr bool is_pressed(MouseButton const button) const { return mouse_action(button) == Action::ePress; }
	constexpr bool is_held(MouseButton const button) const { return mouse_action(button) == Action::eHold; }
	constexpr bool is_repeated(MouseButton const button) const { return mouse_action(button) == Action::eRepeat; }
	constexpr bool is_released(MouseButton const button) const { return mouse_action(button) == Action::eRelease; }

	template <typename T, std::same_as<T>... U>
	constexpr bool chord(T const press, U const... hold) const {
		return is_pressed(press) && (is_held(hold) && ...);
	}
};
} // namespace levk
