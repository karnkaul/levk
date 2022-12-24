#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <levk/impl/desktop_window.hpp>
#include <levk/impl/vulkan_surface.hpp>
#include <levk/util/error.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/zip_ranges.hpp>
#include <levk/window.hpp>
#include <cassert>
#include <unordered_map>

namespace {
levk::DesktopWindow::Storage g_storage{};

constexpr int from(levk::CursorMode const mode) {
	switch (mode) {
	case levk::CursorMode::eDisabled: return GLFW_CURSOR_DISABLED;
	case levk::CursorMode::eHidden: return GLFW_CURSOR_HIDDEN;
	default: return GLFW_CURSOR_NORMAL;
	}
}

constexpr levk::CursorMode to_cursor_mode(int const glfw_cursor_mode) {
	switch (glfw_cursor_mode) {
	case GLFW_CURSOR_DISABLED: return levk::CursorMode::eDisabled;
	case GLFW_CURSOR_HIDDEN: return levk::CursorMode::eHidden;
	default: return levk::CursorMode::eNormal;
	}
}
} // namespace

void levk::window_create(DesktopWindow& out, glm::uvec2 extent, char const* title) {
	if (!glfwInit() || !glfwVulkanSupported()) { throw levk::Error{"TODO: error"}; }

	auto const iextent = glm::ivec2{extent};
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	out.window = glfwCreateWindow(iextent.x, iextent.y, title, {}, {});
	if (!out.window) { throw levk::Error{"TODO: error"}; }
	out.storage = &g_storage;
	auto extension_count = std::uint32_t{};
	auto const extensions = glfwGetRequiredInstanceExtensions(&extension_count);
	if (!extensions) { throw Error{"TODO: error"}; }
	out.storage->vulkan_extensions = {extensions, extensions + extension_count};

	glfwSetWindowCloseCallback(out.window, [](GLFWwindow* w) {
		g_storage.state.flags |= WindowState::eClosed;
		g_storage.state.triggers |= WindowState::eClosed;
		glfwSetWindowShouldClose(w, GLFW_TRUE);
	});
	glfwSetWindowSizeCallback(out.window, [](GLFWwindow*, int x, int y) {
		g_storage.state.extent = glm::ivec2{x, y};
		g_storage.state.triggers |= WindowState::eResized;
	});
	glfwSetFramebufferSizeCallback(out.window, [](GLFWwindow*, int x, int y) {
		g_storage.state.framebuffer = glm::ivec2{x, y};
		g_storage.state.triggers |= WindowState::eResized;
	});
	glfwSetWindowFocusCallback(out.window, [](GLFWwindow*, int gained) {
		g_storage.state.triggers |= WindowState::eInFocus;
		if (gained == GLFW_TRUE) {
			g_storage.state.flags |= WindowState::eInFocus;
		} else {
			g_storage.state.flags &= ~WindowState::eInFocus;
		}
	});
	glfwSetWindowPosCallback(out.window, [](GLFWwindow*, int x, int y) { g_storage.state.position = {x, y}; });
	glfwSetKeyCallback(out.window, [](GLFWwindow*, int key, int, int action, int) {
		if (key >= 0 && static_cast<std::size_t>(key) < g_storage.state.input.keyboard.size()) {
			auto& a = g_storage.state.input.keyboard[static_cast<std::size_t>(key)];
			switch (action) {
			case GLFW_PRESS: a = Action::ePress; break;
			case GLFW_RELEASE: a = Action::eRelease; break;
			case GLFW_REPEAT: a = Action::eRepeat; break;
			default: break;
			}
		}
	});
	glfwSetCharCallback(out.window, [](GLFWwindow*, std::uint32_t codepoint) { g_storage.state.input.codepoints.insert(codepoint); });
	glfwSetMouseButtonCallback(out.window, [](GLFWwindow*, int button, int action, int) {
		if (button >= 0 && static_cast<std::size_t>(button) < g_storage.state.input.mouse.size()) {
			auto& a = g_storage.state.input.mouse[static_cast<std::size_t>(button)];
			switch (action) {
			case GLFW_PRESS: a = Action::ePress; break;
			case GLFW_RELEASE: a = Action::eRelease; break;
			case GLFW_REPEAT: a = Action::eRepeat; break;
			default: break;
			}
		}
	});
	glfwSetScrollCallback(out.window, [](GLFWwindow*, double x, double y) { g_storage.state.input.scroll += glm::dvec2{x, y}; });
	glfwSetCursorPosCallback(out.window, [](GLFWwindow*, double x, double y) { g_storage.state.input.cursor = glm::dvec2{x, y}; });
	glfwSetDropCallback(out.window, [](GLFWwindow*, int count, char const** paths) {
		for (int i = 0; i < count; ++i) { g_storage.drops.push_back(paths[i]); }
	});
	if (glfwRawMouseMotionSupported()) { glfwSetInputMode(out.window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE); }
}

void levk::window_destroy(DesktopWindow& out) {
	glfwDestroyWindow(out.window);
	glfwTerminate();
}

std::span<char const*> levk::window_vulkan_extensions(levk::DesktopWindow const&) { return g_storage.vulkan_extensions; }

void levk::window_make_surface(DesktopWindow const& window, Surface& out, Surface::Source const& source) {
	auto& surface = dynamic_cast<VulkanSurface&>(out);
	auto& instance = dynamic_cast<VulkanSurface::Instance const&>(source);
	auto ret = VkSurfaceKHR{};
	auto const res = glfwCreateWindowSurface(instance.instance, window.window, {}, &ret);
	if (res != VK_SUCCESS || !ret) { throw Error{"TODO: error"}; }
	surface.surface = vk::UniqueSurfaceKHR{ret, instance.instance};
}

glm::uvec2 levk::window_framebuffer_extent(DesktopWindow const& window) {
	auto ret = glm::ivec2{};
	glfwGetFramebufferSize(window.window, &ret.x, &ret.y);
	return ret;
}

void levk::window_show(DesktopWindow& out) { glfwShowWindow(out.window); }
void levk::window_hide(DesktopWindow& out) { glfwHideWindow(out.window); }
void levk::window_close(DesktopWindow& out) { glfwSetWindowShouldClose(out.window, GLFW_TRUE); }

bool levk::window_is_open(DesktopWindow const& window) { return !glfwWindowShouldClose(window.window); }

levk::WindowState const& levk::window_state(DesktopWindow const&) { return g_storage.state; }

void levk::window_poll(DesktopWindow&) {
	g_storage.state.triggers = {};
	g_storage.state.input.codepoints = {};
	g_storage.state.input.scroll = {};
	g_storage.drops.clear();
	auto const update = [](auto& actions) {
		for (auto& action : actions) {
			switch (action) {
			case Action::eRelease: action = Action::eNone; break;
			case Action::ePress:
			case Action::eHold:
			case Action::eRepeat: action = Action::eHold; break;
			default: break;
			}
		}
	};
	update(g_storage.state.input.keyboard);
	update(g_storage.state.input.mouse);
	glfwPollEvents();
	g_storage.state.drops = g_storage.drops;
}

char const* levk::window_clipboard(DesktopWindow const& window) { return glfwGetClipboardString(window.window); }
void levk::window_set_clipboard(DesktopWindow& out, char const* text) { glfwSetClipboardString(out.window, text); }

levk::CursorMode levk::window_cursor_mode(DesktopWindow const& window) { return to_cursor_mode(glfwGetInputMode(window.window, GLFW_CURSOR)); }
void levk::window_set_cursor_mode(DesktopWindow& out, CursorMode const mode) { glfwSetInputMode(out.window, GLFW_CURSOR, from(mode)); }
