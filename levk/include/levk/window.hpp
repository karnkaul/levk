#pragma once
#include <levk/surface.hpp>
#include <levk/util/ptr.hpp>
#include <levk/window_state.hpp>
#include <memory>
#include <span>

namespace levk {
class Window {
  public:
	using State = WindowState;

	template <typename T>
	Window(T t) : m_model(std::make_unique<Model<T>>(std::move(t))) {}
	~Window() {
		if (m_model) { destroy(); }
	}

	Window(Window&&) = default;
	Window& operator=(Window&&) = default;

	void create(glm::uvec2 extent, char const* title) { m_model->create(extent, title); }
	void destroy() { m_model->destroy(); }
	std::span<char const*> vulkan_extensions() const { return m_model->vulkan_extensions(); }
	void make_surface(Surface& out, Surface::Source const& source) const { return m_model->make_surface(out, source); }

	glm::uvec2 framebuffer_extent() const { return m_model->framebuffer_extent(); }
	void show() { m_model->show(); }
	void hide() { m_model->hide(); }
	void close() { m_model->close(); }
	bool is_open() const { return m_model->is_open(); }
	void poll() { m_model->poll(); }
	State const& state() const { return m_model->state(); }
	char const* clipboard() const { return m_model->clipboard(); }
	void set_clipboard(char const* text) { m_model->set_clipboard(text); }
	CursorMode cursor_mode() const { return m_model->cursor_mode(); }
	void set_cursor_mode(CursorMode mode) { m_model->set_cursor_mode(mode); }

	template <typename T>
	Ptr<T const> as() const {
		if (auto t = dynamic_cast<Model<T> const*>(m_model.get())) { return &t->impl; }
		return {};
	}

  private:
	struct Base {
		virtual ~Base() = default;

		virtual void create(glm::uvec2 extent, char const* title) = 0;
		virtual void destroy() = 0;
		virtual std::span<char const*> vulkan_extensions() const = 0;
		virtual void make_surface(Surface& out, Surface::Source const& source) const = 0;
		virtual glm::uvec2 framebuffer_extent() const = 0;
		virtual void show() = 0;
		virtual void hide() = 0;
		virtual void close() = 0;
		virtual bool is_open() const = 0;
		virtual void poll() = 0;
		virtual State const& state() const = 0;
		virtual char const* clipboard() const = 0;
		virtual void set_clipboard(char const* text) = 0;
		virtual CursorMode cursor_mode() const = 0;
		virtual void set_cursor_mode(CursorMode mode) = 0;
	};

	template <typename T>
	struct Model : public Base {
		T impl;
		Model(T&& impl) : impl(std::move(impl)) {}

		void create(glm::uvec2 extent, char const* title) final { window_create(impl, extent, title); }
		void destroy() final { window_destroy(impl); }
		std::span<char const*> vulkan_extensions() const { return window_vulkan_extensions(impl); }
		void make_surface(Surface& out, Surface::Source const& source) const final { return window_make_surface(impl, out, source); }
		glm::uvec2 framebuffer_extent() const final { return window_framebuffer_extent(impl); }
		void show() final { window_show(impl); }
		void hide() final { window_hide(impl); }
		void close() final { window_close(impl); }
		bool is_open() const final { return window_is_open(impl); }
		void poll() final { window_poll(impl); }
		State const& state() const final { return window_state(impl); }
		char const* clipboard() const final { return window_clipboard(impl); }
		void set_clipboard(char const* text) final { window_set_clipboard(impl, text); }
		CursorMode cursor_mode() const final { return window_cursor_mode(impl); }
		void set_cursor_mode(CursorMode mode) final { window_set_cursor_mode(impl, mode); }
	};

	std::unique_ptr<Base> m_model{};
};

struct WindowFactory {
	virtual Window make() const = 0;
};

struct DesktopWindowFactory : WindowFactory {
	Window make() const override;
};
} // namespace levk
