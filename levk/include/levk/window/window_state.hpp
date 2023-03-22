#pragma once
#include <levk/util/bit_flags.hpp>
#include <levk/window/input.hpp>
#include <string>

namespace levk {
using Extent2D = glm::uvec2;

enum class WindowFlag : std::uint32_t {
	eInFocus = 1 << 0,
	eMaximized = 1 << 1,
	eMinimized = 1 << 2,
	eClosed = 1 << 3,
	eResized = 1 << 4,
};

struct WindowState {
	using Flag = WindowFlag;

	Input input{};
	std::span<std::string const> drops{};
	Extent2D extent{};
	Extent2D framebuffer{};
	glm::ivec2 position{};
	BitFlags<Flag, std::uint32_t> flags{};
	BitFlags<Flag, std::uint32_t> triggers{};

	constexpr bool is_in_focus() const { return flags.test(Flag::eInFocus); }
	constexpr bool is_maximized() const { return flags.test(Flag::eMaximized); }
	constexpr bool is_minimized() const { return flags.test(Flag::eMinimized); }
	constexpr bool is_closed() const { return flags.test(Flag::eClosed); }

	constexpr bool focus_changed() const { return triggers.test(Flag::eInFocus); }
	constexpr bool maximize_changed() const { return triggers.test(Flag::eMaximized); }
	constexpr bool minimize_changed() const { return triggers.test(Flag::eMinimized); }
	constexpr bool closed() const { return triggers.test(Flag::eClosed); }
	constexpr bool resized() const { return triggers.test(Flag::eResized); }

	constexpr glm::vec2 display_ratio() const {
		if (extent.x == 0 || extent.y == 0 || framebuffer.x == 0 || framebuffer.y == 0) { return {}; }
		return glm::vec2{framebuffer} / glm::vec2{extent};
	}
};
} // namespace levk
