#include <levk/window/window.hpp>
#include <window/glfw/window.hpp>

namespace levk {
void Window::Deleter::operator()(glfw::Window const* ptr) const { delete ptr; }

Window::Window(glm::uvec2 extent, char const* title) : m_glfw_window(std::unique_ptr<glfw::Window, Deleter>(new glfw::Window{extent, title})) {}

glm::uvec2 Window::framebuffer_extent() const { return m_glfw_window->framebuffer_extent(); }
void Window::show() { return m_glfw_window->show(); }
void Window::hide() { return m_glfw_window->hide(); }
void Window::close() { return m_glfw_window->close(); }
bool Window::is_open() const { return m_glfw_window->is_open(); }
void Window::poll() { return m_glfw_window->poll(); }
auto Window::state() const -> State const& { return m_glfw_window->state(); }
char const* Window::clipboard() const { return m_glfw_window->clipboard(); }
void Window::set_clipboard(char const* text) { return m_glfw_window->set_clipboard(text); }
CursorMode Window::cursor_mode() const { return m_glfw_window->cursor_mode(); }
void Window::set_cursor_mode(CursorMode mode) { return m_glfw_window->set_cursor_mode(mode); }
} // namespace levk
