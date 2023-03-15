#pragma once
#include <glm/vec2.hpp>
#include <levk/geometry.hpp>
#include <levk/renderer.hpp>
#include <levk/skinned_mesh.hpp>
#include <levk/static_mesh.hpp>
#include <levk/texture.hpp>
#include <levk/util/ptr.hpp>
#include <memory>

namespace levk {
struct RenderInfo;

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
	Info const& info() const { return m_model->info(); }
	bool set_vsync(Vsync::Type desired) { return m_model->set_vsync(desired); }
	bool set_render_scale(float scale) { return m_model->set_render_scale(scale); }

	void render(Renderer const& renderer, AssetProviders const& providers, Camera const& camera, Lights const& lights, glm::uvec2 framebuffer_extent);

	MeshGeometry make_static_mesh_geometry(Geometry::Packed const& geometry) { return m_model->make_static_mesh_geometry(geometry); }
	MeshGeometry make_skinned_mesh_geometry(Geometry::Packed const& geometry, MeshJoints joints = {}) {
		return m_model->make_skinned_mesh_geometry(geometry, joints);
	}
	MeshGeometry make_ui_mesh_geometry(Geometry::Packed const& geometry) { return m_model->make_ui_mesh_geometry(geometry); }

	Texture make_texture(Image::View image, Texture::CreateInfo create_info = {}) { return m_model->make_texture(image, std::move(create_info)); }

	RenderStats stats() const { return m_model->stats(); }

	template <typename T>
	Ptr<T> as() const {
		if (auto* p = dynamic_cast<Model<T>*>(m_model.get())) { return &p->impl; }
		return {};
	}

	RenderMode default_render_mode{};
	Rgba clear_colour{black_v};

  private:
	struct Base {
		virtual ~Base() = default;

		virtual void create(CreateInfo const& create_info) = 0;
		virtual void destroy() = 0;
		virtual Info const& info() const = 0;
		virtual bool set_vsync(Vsync::Type) = 0;
		virtual bool set_render_scale(float) = 0;

		virtual void render(RenderInfo const& info) const = 0;

		virtual MeshGeometry make_static_mesh_geometry(Geometry::Packed const& geometry) = 0;
		virtual MeshGeometry make_skinned_mesh_geometry(Geometry::Packed const& geometry, MeshJoints joints) = 0;
		virtual MeshGeometry make_ui_mesh_geometry(Geometry::Packed const& geometry) = 0;
		virtual Texture make_texture(Image::View image, Texture::CreateInfo info) = 0;

		virtual RenderStats stats() const = 0;
	};

	template <typename T>
	struct Model final : Base {
		T impl;
		Model(T&& t) : impl(std::move(t)) {}

		void create(CreateInfo const& create_info) final { gfx_create_device(impl, create_info); }
		void destroy() final { gfx_destroy_device(impl); }
		Info const& info() const final { return gfx_info(impl); }
		bool set_vsync(Vsync::Type vsync) final { return gfx_set_vsync(impl, vsync); }
		bool set_render_scale(float scale) final { return gfx_set_render_scale(impl, scale); }

		void render(RenderInfo const& info) const final { gfx_render(impl, info); }

		MeshGeometry make_static_mesh_geometry(Geometry::Packed const& geometry) final { return gfx_make_static_mesh_geometry(impl, geometry); }
		MeshGeometry make_skinned_mesh_geometry(Geometry::Packed const& geometry, MeshJoints joints) final {
			return gfx_make_skinned_mesh_geometry(impl, geometry, joints);
		}
		MeshGeometry make_ui_mesh_geometry(Geometry::Packed const& geometry) final { return gfx_make_ui_mesh_geometry(impl, geometry); }

		Texture make_texture(Image::View image, Texture::CreateInfo create_info) final { return gfx_make_texture(impl, std::move(create_info), image); }

		RenderStats stats() const final { return gfx_render_stats(impl); }
	};

	std::unique_ptr<Base> m_model{};
};

struct RenderInfo {
	Renderer const& renderer;
	AssetProviders const& providers;
	Camera const& camera;
	Lights const& lights;
	Extent2D extent;
	Rgba clear;
	RenderMode default_render_mode;
};

struct GraphicsDeviceFactory {
	virtual GraphicsDevice make() const = 0;
};

struct VulkanDeviceFactory : GraphicsDeviceFactory {
	GraphicsDevice make() const override;
};
} // namespace levk
