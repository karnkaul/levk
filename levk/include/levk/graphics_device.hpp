#pragma once
#include <glm/vec2.hpp>
#include <levk/geometry.hpp>
#include <levk/graphics_common.hpp>
#include <levk/skinned_mesh.hpp>
#include <levk/static_mesh.hpp>
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
	Info const& info() const { return m_model->info(); }
	bool set_render_scale(float scale) { return m_model->set_render_scale(scale); }

	void render(GraphicsRenderer& renderer, Camera const& camera, Lights const& lights, glm::uvec2 extent, Rgba clear);

	MeshGeometry make_mesh_geometry(Geometry::Packed const& geometry, MeshJoints joints = {}) { return m_model->make_mesh_geometry(geometry, joints); }
	Texture make_texture(Image::View image, Texture::CreateInfo create_info = {}) { return m_model->make_texture(image, std::move(create_info)); }

	void render(StaticMesh const& mesh, RenderResources const& resources, std::span<Transform const> instances, glm::mat4 const& parent = matrix_identity_v);
	void render(refactor::StaticMesh const& mesh, refactor::RenderResources const& resources, std::span<Transform const> instances,
				glm::mat4 const& parent = matrix_identity_v);
	void render(SkinnedMesh const& mesh, RenderResources const& resources, std::span<glm::mat4 const> joints);
	void render(refactor::SkinnedMesh const& mesh, refactor::RenderResources const& resources, std::span<glm::mat4 const> joints);

	template <typename T>
	Ptr<T> as() const {
		if (auto* p = dynamic_cast<Model<T>*>(m_model.get())) { return &p->impl; }
		return {};
	}

	RenderMode default_render_mode{};

  private:
	struct Base {
		virtual ~Base() = default;

		virtual void create(CreateInfo const& create_info) = 0;
		virtual void destroy() = 0;
		virtual Info const& info() const = 0;
		virtual bool set_render_scale(float) = 0;

		virtual void render(RenderInfo const& info) = 0;

		virtual MeshGeometry make_mesh_geometry(Geometry::Packed const& geometry, MeshJoints joints) = 0;
		virtual Texture make_texture(Image::View image, Texture::CreateInfo info) = 0;

		virtual void render(StaticMeshRenderInfo const&) = 0;
		virtual void render(refactor::StaticMeshRenderInfo const&) = 0;
		virtual void render(SkinnedMeshRenderInfo const&) = 0;
		virtual void render(refactor::SkinnedMeshRenderInfo const&) = 0;
	};

	template <typename T>
	struct Model : Base {
		T impl;
		Model(T&& t) : impl(std::move(t)) {}

		void create(CreateInfo const& create_info) final { gfx_create_device(impl, create_info); }
		void destroy() final { gfx_destroy_device(impl); }
		Info const& info() const final { return gfx_info(impl); }
		bool set_render_scale(float scale) final { return gfx_set_render_scale(impl, scale); }

		void render(RenderInfo const& info) final { gfx_render(impl, info); }

		MeshGeometry make_mesh_geometry(Geometry::Packed const& geometry, MeshJoints joints) final { return gfx_make_mesh_geometry(impl, geometry, joints); }
		Texture make_texture(Image::View image, Texture::CreateInfo create_info) final { return gfx_make_texture(impl, std::move(create_info), image); }

		void render(StaticMeshRenderInfo const& info) final { gfx_render(impl, info); }
		void render(refactor::StaticMeshRenderInfo const& info) final { gfx_render(impl, info); }
		void render(SkinnedMeshRenderInfo const& info) final { gfx_render(impl, info); }
		void render(refactor::SkinnedMeshRenderInfo const& info) final { gfx_render(impl, info); }
	};

	std::unique_ptr<Base> m_model{};
};
} // namespace levk
