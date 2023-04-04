#pragma once
#include <levk/util/ptr.hpp>
#include <levk/window/window_state.hpp>
#include <memory>
#include <span>

namespace levk {
namespace glfw {
struct Window;
}

class Window {
  public:
	using State = WindowState;

	Window(glm::uvec2 extent, char const* title);

	glm::uvec2 framebuffer_extent() const;
	void show();
	void hide();
	void close();
	bool is_open() const;
	void poll();
	State const& state() const;
	char const* clipboard() const;
	void set_clipboard(char const* text);
	CursorMode cursor_mode() const;
	void set_cursor_mode(CursorMode mode);

	void set_title(char const* title);
	void set_extent(Extent2D extent);

	Ptr<glfw::Window> glfw_window() const { return m_impl.get(); }

  private:
	struct Deleter {
		void operator()(glfw::Window const*) const;
	};

	std::unique_ptr<glfw::Window, Deleter> m_impl{};

	friend struct Wsi;
};
} // namespace levk
