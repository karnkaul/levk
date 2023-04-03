#include <levk/util/error.hpp>
#include <window/glfw/window.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace levk::glfw {
namespace {
constexpr int from(CursorMode const mode) {
	switch (mode) {
	case CursorMode::eDisabled: return GLFW_CURSOR_DISABLED;
	case CursorMode::eHidden: return GLFW_CURSOR_HIDDEN;
	default: return GLFW_CURSOR_NORMAL;
	}
}

constexpr CursorMode to_cursor_mode(int const glfw_cursor_mode) {
	switch (glfw_cursor_mode) {
	case GLFW_CURSOR_DISABLED: return CursorMode::eDisabled;
	case GLFW_CURSOR_HIDDEN: return CursorMode::eHidden;
	default: return CursorMode::eNormal;
	}
}

constexpr glm::vec2 screen_to_world(glm::vec2 screen, glm::vec2 extent, glm::vec2 display_ratio) {
	auto const he = 0.5f * glm::vec2{extent};
	auto ret = glm::vec2{screen.x - he.x, he.y - screen.y};
	auto const dr = display_ratio;
	if (dr.x == 0.0f || dr.y == 0.0f) { return ret; }
	return dr * ret;
}

Window::Storage g_storage{};
} // namespace

Window::Window(glm::uvec2 extent, char const* title) {
	if (g_storage.instance) { throw Error{"[Window] Multiple instances are not supported"}; }
	if (!glfwInit() || !glfwVulkanSupported()) { throw Error{"[Window] Vulkan not supported"}; }

	g_storage.instance = this;

	auto const iextent = glm::ivec2{extent};
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	window = glfwCreateWindow(iextent.x, iextent.y, title, {}, {});
	if (!window) { throw Error{"[Window] Failed to create window instance"}; }

	glfwSetWindowCloseCallback(window, [](GLFWwindow* w) {
		g_storage.state.flags.set(WindowFlag::eClosed);
		g_storage.state.triggers.set(WindowFlag::eClosed);
		glfwSetWindowShouldClose(w, GLFW_TRUE);
	});
	glfwSetWindowSizeCallback(window, [](GLFWwindow*, int x, int y) {
		g_storage.state.extent = glm::ivec2{x, y};
		g_storage.state.triggers.set(WindowFlag::eResized);
	});
	glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int x, int y) {
		g_storage.state.framebuffer = glm::ivec2{x, y};
		g_storage.state.triggers.set(WindowFlag::eResized);
	});
	glfwSetWindowFocusCallback(window, [](GLFWwindow*, int gained) {
		g_storage.state.triggers.set(WindowFlag::eInFocus);
		if (gained == GLFW_TRUE) {
			g_storage.state.flags.set(WindowFlag::eInFocus);
		} else {
			g_storage.state.flags.reset(WindowFlag::eInFocus);
		}
	});
	glfwSetWindowPosCallback(window, [](GLFWwindow*, int x, int y) { g_storage.state.position = {x, y}; });
	glfwSetKeyCallback(window, [](GLFWwindow*, int key, int, int action, int) {
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
	glfwSetCharCallback(window, [](GLFWwindow*, std::uint32_t codepoint) { g_storage.state.input.codepoints.insert(codepoint); });
	glfwSetMouseButtonCallback(window, [](GLFWwindow*, int button, int action, int) {
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
	glfwSetScrollCallback(window, [](GLFWwindow*, double x, double y) { g_storage.state.input.scroll += glm::dvec2{x, y}; });
	glfwSetCursorPosCallback(window, [](GLFWwindow*, double x, double y) { g_storage.raw_cursor_position = glm::dvec2{x, y}; });
	glfwSetDropCallback(window, [](GLFWwindow*, int count, char const** paths) {
		for (int i = 0; i < count; ++i) { g_storage.drops.push_back(paths[i]); }
	});
	if (glfwRawMouseMotionSupported()) { glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE); }

	g_storage.state.extent = extent;
}

Window::~Window() noexcept {
	glfwDestroyWindow(window);
	glfwTerminate();
	g_storage.instance = {};
}

std::vector<char const*> Window::vulkan_extensions() {
	auto extension_count = std::uint32_t{};
	auto const extensions = glfwGetRequiredInstanceExtensions(&extension_count);
	if (!extensions) { throw Error{"[Window] Failed to find required Vulkan extensions"}; }
	return {extensions, extensions + extension_count};
}

vk::UniqueSurfaceKHR Window::make_surface(vk::Instance instance) const {
	auto ret = VkSurfaceKHR{};
	auto const res = glfwCreateWindowSurface(static_cast<VkInstance>(instance), window, {}, &ret);
	if (res != VK_SUCCESS || !ret) { throw Error{"[Wsi] Failed to create Vulkan Surface"}; }
	return vk::UniqueSurfaceKHR{ret, instance};
}

glm::uvec2 Window::framebuffer_extent() const {
	auto ret = glm::ivec2{};
	glfwGetFramebufferSize(window, &ret.x, &ret.y);
	return ret;
}

void Window::show() { glfwShowWindow(window); }
void Window::hide() { glfwHideWindow(window); }
void Window::close() { glfwSetWindowShouldClose(window, GLFW_TRUE); }

bool Window::is_open() const { return !glfwWindowShouldClose(window); }

WindowState const& Window::state() const { return g_storage.state; }

void Window::poll() {
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
	g_storage.state.input.cursor = screen_to_world(g_storage.raw_cursor_position, g_storage.state.extent, g_storage.state.display_ratio());
	g_storage.state.input.ui_space = g_storage.state.framebuffer;
}

char const* Window::clipboard() const { return glfwGetClipboardString(window); }
void Window::set_clipboard(char const* text) { glfwSetClipboardString(window, text); }

CursorMode Window::cursor_mode() const { return to_cursor_mode(glfwGetInputMode(window, GLFW_CURSOR)); }
void Window::set_cursor_mode(CursorMode const mode) { glfwSetInputMode(window, GLFW_CURSOR, from(mode)); }
} // namespace levk::glfw
