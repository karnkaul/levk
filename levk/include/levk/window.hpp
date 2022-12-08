#pragma once
#include <glm/vec2.hpp>
#include <levk/event.hpp>
#include <levk/surface.hpp>
#include <levk/util/ptr.hpp>
#include <memory>
#include <span>

namespace levk {
class Window {
  public:
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
	std::span<Event const> poll() { return m_model->poll(); }

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
		virtual std::span<Event const> poll() = 0;
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
		std::span<Event const> poll() final { return window_poll(impl); }
	};

	std::unique_ptr<Base> m_model{};
};
} // namespace levk
