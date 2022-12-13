#pragma once
#include <levk/surface.hpp>
#include <levk/util/ptr.hpp>
#include <levk/window_state.hpp>
#include <span>
#include <vector>

struct GLFWwindow;

namespace levk {
struct DesktopWindow {
	struct Storage;

	Ptr<GLFWwindow> window{};
	Ptr<Storage> storage{};
};

struct DesktopWindow::Storage {
	std::vector<char const*> vulkan_extensions{};
	std::vector<std::string> drops{};
	WindowState state{};
};

void window_create(DesktopWindow& out, glm::uvec2 extent, char const* title);
void window_destroy(DesktopWindow& out);
std::span<char const*> window_vulkan_extensions(DesktopWindow const& window);
void window_make_surface(DesktopWindow const& window, Surface& out, Surface::Source const& source);
glm::uvec2 window_framebuffer_extent(DesktopWindow const& window);
void window_show(DesktopWindow& out);
void window_hide(DesktopWindow& out);
void window_close(DesktopWindow& out);
bool window_is_open(DesktopWindow const& window);
WindowState const& window_state(DesktopWindow const& window);
void window_poll(DesktopWindow& out);
char const* window_clipboard(DesktopWindow const& window);
void window_set_clipboard(DesktopWindow& out, char const* text);
} // namespace levk
