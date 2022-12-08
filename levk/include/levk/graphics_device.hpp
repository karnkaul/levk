#pragma once
#include <glm/vec2.hpp>
#include <levk/graphics_common.hpp>
#include <memory>

namespace levk {
class GraphicsDevice {
  public:
	using CreateInfo = GraphicsDeviceCreateInfo;
	using Info = GraphicsDeviceInfo;

	template <typename T>
	GraphicsDevice(T t) : m_model(std::make_unique<Model<T>>(std::move(t))) {}

	~GraphicsDevice() {
		if (m_model) { destroy(); }
	}

	GraphicsDevice(GraphicsDevice&&) = default;
	GraphicsDevice& operator=(GraphicsDevice&&) = default;

	void create(CreateInfo const& create_info) { m_model->create(create_info); }
	void destroy() { m_model->destroy(); }
	void render(GraphicsRenderer& renderer, glm::uvec2 framebuffer_extent) { m_model->render(renderer, framebuffer_extent); }

  private:
	struct Base {
		virtual ~Base() = default;

		virtual void create(CreateInfo const& create_info) = 0;
		virtual void destroy() = 0;
		virtual Info const& info() const = 0;
		virtual void render(GraphicsRenderer& renderer, glm::uvec2 framebuffer_extent) = 0;
	};

	template <typename T>
	struct Model : Base {
		T impl;
		Model(T&& t) : impl(std::move(t)) {}

		void create(CreateInfo const& create_info) final { gfx_create_device(impl, create_info); }
		void destroy() final { gfx_destroy_device(impl); }
		Info const& info() const final { return gfx_info(impl); }

		void render(GraphicsRenderer& renderer, glm::uvec2 framebuffer_extent) final { gfx_render(impl, renderer, framebuffer_extent); }
	};

	std::unique_ptr<Base> m_model{};
};
} // namespace levk
