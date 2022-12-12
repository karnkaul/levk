#pragma once
#include <glm/vec2.hpp>
#include <levk/geometry.hpp>
#include <levk/graphics_common.hpp>
#include <levk/mesh.hpp>
#include <levk/texture.hpp>
#include <levk/util/ptr.hpp>
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
	void render(GraphicsRenderer& renderer, Camera const& camera, glm::uvec2 extent, Rgba clear) { m_model->render(renderer, camera, extent, clear); }

	MeshGeometry make_mesh_geometry(Geometry const& geometry, MeshJoints joints = {}) { return m_model->make_mesh_geometry(geometry, joints); }
	Texture make_texture(Image::View image, Texture::CreateInfo const& create_info = {}) { return m_model->make_texture(image, create_info); }

	void render(Mesh const& mesh) { m_model->render(mesh); }

	template <typename T>
	Ptr<T> as() const {
		if (auto* p = dynamic_cast<Model<T>*>(m_model.get())) { return &p->impl; }
		return {};
	}

  private:
	struct Base {
		virtual ~Base() = default;

		virtual void create(CreateInfo const& create_info) = 0;
		virtual void destroy() = 0;
		virtual Info const& info() const = 0;
		virtual void render(GraphicsRenderer& renderer, Camera const& camera, glm::uvec2 extent, Rgba clear) = 0;

		virtual MeshGeometry make_mesh_geometry(Geometry const& geometry, MeshJoints const& joints) = 0;
		virtual Texture make_texture(Image::View image, Texture::CreateInfo const& info) = 0;

		virtual void render(Mesh const& mesh) = 0;
	};

	template <typename T>
	struct Model : Base {
		T impl;
		Model(T&& t) : impl(std::move(t)) {}

		void create(CreateInfo const& create_info) final { gfx_create_device(impl, create_info); }
		void destroy() final { gfx_destroy_device(impl); }
		Info const& info() const final { return gfx_info(impl); }

		void render(GraphicsRenderer& renderer, Camera const& camera, glm::uvec2 extent, Rgba clear) final {
			gfx_render(impl, renderer, camera, extent, clear);
		}

		MeshGeometry make_mesh_geometry(Geometry const& geometry, MeshJoints const& joints) final { return gfx_make_mesh_geometry(impl, geometry, joints); }
		Texture make_texture(Image::View image, Texture::CreateInfo const& create_info) final { return gfx_make_texture(impl, create_info, image); }

		void render(Mesh const& mesh) final { gfx_render_mesh(impl, mesh); }
	};

	std::unique_ptr<Base> m_model{};
};
} // namespace levk
