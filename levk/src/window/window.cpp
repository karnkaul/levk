#include <levk/util/error.hpp>
#include <levk/window/window.hpp>
#include <window/glfw/window.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace levk {
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

glfw::Window::Storage g_storage{};

bool match(GLFWwindow* in) { return g_storage.instance && g_storage.instance->window == in; }
} // namespace

void Window::Deleter::operator()(glfw::Window const* ptr) const {
	if (ptr) {
		glfwDestroyWindow(ptr->window);
		glfwTerminate();
		g_storage.instance = {};
	}
	delete ptr;
}

Window::Window(glm::uvec2 extent, char const* title) : m_impl(new glfw::Window{}) {
	if (g_storage.instance) { throw Error{"[Window] Multiple instances are not supported"}; }
	if (!glfwInit() || !glfwVulkanSupported()) { throw Error{"[Window] Vulkan not supported"}; }

	g_storage.instance = m_impl.get();

	auto const iextent = glm::ivec2{extent};
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	m_impl->window = glfwCreateWindow(iextent.x, iextent.y, title, {}, {});
	if (!m_impl->window) { throw Error{"[Window] Failed to create window instance"}; }

	glfwSetWindowCloseCallback(m_impl->window, [](GLFWwindow* w) {
		if (!match(w)) { return; }
		g_storage.state.flags.set(WindowFlag::eClosed);
		g_storage.state.triggers.set(WindowFlag::eClosed);
		glfwSetWindowShouldClose(w, GLFW_TRUE);
	});
	glfwSetWindowSizeCallback(m_impl->window, [](GLFWwindow* w, int x, int y) {
		if (!match(w)) { return; }

		g_storage.state.extent = glm::ivec2{x, y};
		g_storage.state.triggers.set(WindowFlag::eResized);
	});
	glfwSetFramebufferSizeCallback(m_impl->window, [](GLFWwindow* w, int x, int y) {
		if (!match(w)) { return; }

		g_storage.state.framebuffer = glm::ivec2{x, y};
		g_storage.state.triggers.set(WindowFlag::eResized);
	});
	glfwSetWindowFocusCallback(m_impl->window, [](GLFWwindow* w, int gained) {
		if (!match(w)) { return; }
		g_storage.state.triggers.set(WindowFlag::eInFocus);
		if (gained == GLFW_TRUE) {
			g_storage.state.flags.set(WindowFlag::eInFocus);
		} else {
			g_storage.state.flags.reset(WindowFlag::eInFocus);
		}
	});
	glfwSetWindowPosCallback(m_impl->window, [](GLFWwindow* w, int x, int y) {
		if (!match(w)) { return; }
		g_storage.state.position = {x, y};
	});
	glfwSetKeyCallback(m_impl->window, [](GLFWwindow* w, int key, int, int action, int) {
		if (!match(w)) { return; }
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
	glfwSetCharCallback(m_impl->window, [](GLFWwindow* w, std::uint32_t codepoint) {
		if (!match(w)) { return; }
		g_storage.state.input.codepoints.insert(codepoint);
	});
	glfwSetMouseButtonCallback(m_impl->window, [](GLFWwindow* w, int button, int action, int) {
		if (!match(w)) { return; }
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
	glfwSetScrollCallback(m_impl->window, [](GLFWwindow* w, double x, double y) {
		if (!match(w)) { return; }
		g_storage.state.input.scroll += glm::dvec2{x, y};
	});
	glfwSetCursorPosCallback(m_impl->window, [](GLFWwindow* w, double x, double y) {
		if (!match(w)) { return; }
		g_storage.raw_cursor_position = glm::dvec2{x, y};
	});
	glfwSetDropCallback(m_impl->window, [](GLFWwindow* w, int count, char const** paths) {
		if (!match(w)) { return; }
		for (int i = 0; i < count; ++i) { g_storage.drops.push_back(paths[i]); }
	});
	if (glfwRawMouseMotionSupported()) { glfwSetInputMode(m_impl->window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE); }

	g_storage.state.extent = extent;
}

glm::uvec2 Window::framebuffer_extent() const { return m_impl->framebuffer_extent(); }
glm::uvec2 Window::window_extent() const { return m_impl->window_extent(); }

void Window::show() { glfwShowWindow(m_impl->window); }
void Window::hide() { glfwHideWindow(m_impl->window); }
void Window::close() { glfwSetWindowShouldClose(m_impl->window, GLFW_TRUE); }

bool Window::is_open() const { return !glfwWindowShouldClose(m_impl->window); }

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

char const* Window::clipboard() const { return glfwGetClipboardString(m_impl->window); }
void Window::set_clipboard(char const* text) { glfwSetClipboardString(m_impl->window, text); }

CursorMode Window::cursor_mode() const { return to_cursor_mode(glfwGetInputMode(m_impl->window, GLFW_CURSOR)); }
void Window::set_cursor_mode(CursorMode const mode) { glfwSetInputMode(m_impl->window, GLFW_CURSOR, from(mode)); }

void Window::set_title(char const* title) { glfwSetWindowTitle(m_impl->window, title); }

void Window::set_extent(Extent2D extent) {
	glm::ivec2 const size = extent;
	glfwSetWindowSize(m_impl->window, size.x, size.y);
}

void Window::lock_aspect_ratio() {
	glm::ivec2 const size = window_extent();
	glfwSetWindowAspectRatio(m_impl->window, size.x, size.y);
}

void Window::unlock_aspect_ratio() { glfwSetWindowAspectRatio(m_impl->window, GLFW_DONT_CARE, GLFW_DONT_CARE); }
} // namespace levk
