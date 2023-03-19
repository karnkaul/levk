#pragma once
#include <glm/vec2.hpp>
#include <levk/geometry.hpp>
#include <levk/graphics/primitive.hpp>
#include <levk/graphics/renderer.hpp>
#include <levk/graphics/texture.hpp>
#include <levk/util/ptr.hpp>
#include <memory>

namespace levk {
struct RenderInfo;
class AssetProviders;

class RenderDevice {
  public:
	using CreateInfo = GraphicsDeviceCreateInfo;
	using Info = GraphicsDeviceInfo;

	template <typename T>
	RenderDevice(T t) : m_model(std::make_unique<Model<T>>(std::move(t))) {}

	~RenderDevice() {
		if (m_model) { destroy(); }
	}

	RenderDevice(RenderDevice&&) = default;
	RenderDevice& operator=(RenderDevice&&) = default;

	void create(CreateInfo const& create_info) { m_model->create(create_info); }
	void destroy() { m_model->destroy(); }
	Info const& info() const { return m_model->info(); }
	bool set_vsync(Vsync::Type desired) { return m_model->set_vsync(desired); }
	bool set_render_scale(float scale) { return m_model->set_render_scale(scale); }

	void render(Renderer const& renderer, AssetProviders const& providers, Camera const& camera, Lights const& lights, glm::uvec2 framebuffer_extent);

	std::unique_ptr<Primitive::Dynamic> make_dynamic() const { return m_model->make_dynamic(); }
	std::unique_ptr<Primitive::Static> make_static(Geometry::Packed const& geometry) const { return m_model->make_static(geometry); }
	std::unique_ptr<Primitive::Skinned> make_skinned(Geometry::Packed const& geometry, MeshJoints const& joints) const {
		return m_model->make_skinned(geometry, joints);
	}

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
		virtual Texture make_texture(Image::View image, Texture::CreateInfo info) = 0;

		virtual std::unique_ptr<Primitive::Dynamic> make_dynamic() const = 0;
		virtual std::unique_ptr<Primitive::Static> make_static(Geometry::Packed const&) const = 0;
		virtual std::unique_ptr<Primitive::Skinned> make_skinned(Geometry::Packed const&, MeshJoints const&) const = 0;

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

		Texture make_texture(Image::View image, Texture::CreateInfo create_info) final { return gfx_make_texture(impl, std::move(create_info), image); }

		std::unique_ptr<Primitive::Dynamic> make_dynamic() const final { return gfx_make_dynamic(impl); }
		std::unique_ptr<Primitive::Static> make_static(Geometry::Packed const& geometry) const final { return gfx_make_static(impl, geometry); }
		std::unique_ptr<Primitive::Skinned> make_skinned(Geometry::Packed const& geometry, MeshJoints const& joints) const final {
			return gfx_make_skinned(impl, geometry, joints);
		}

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
	virtual RenderDevice make() const = 0;
};

struct VulkanDeviceFactory : GraphicsDeviceFactory {
	RenderDevice make() const override;
};
} // namespace levk
