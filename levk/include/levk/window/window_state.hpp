#pragma once
#include <levk/window/input.hpp>
#include <string>

namespace levk {
using Extent2D = glm::uvec2;

struct WindowState {
	enum Flag : std::uint32_t {
		eInFocus = 1 << 0,
		eMaximized = 1 << 1,
		eMinimized = 1 << 2,
		eClosed = 1 << 3,
		eResized = 1 << 4,
	};

	Input input{};
	std::span<std::string const> drops{};
	Extent2D extent{};
	Extent2D framebuffer{};
	glm::ivec2 position{};
	std::uint32_t flags{};
	std::uint32_t triggers{};

	constexpr bool is_in_focus() const { return flags & eInFocus; }
	constexpr bool is_maximized() const { return flags & eMaximized; }
	constexpr bool is_minimized() const { return flags & eMinimized; }
	constexpr bool is_closed() const { return flags & eClosed; }

	constexpr bool focus_changed() const { return triggers & eInFocus; }
	constexpr bool maximize_changed() const { return triggers & eMaximized; }
	constexpr bool minimize_changed() const { return triggers & eMinimized; }
	constexpr bool closed() const { return triggers & eClosed; }
	constexpr bool resized() const { return triggers & eResized; }
};
} // namespace levk
