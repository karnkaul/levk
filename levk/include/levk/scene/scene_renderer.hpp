#pragma once
#include <levk/graphics/render_list.hpp>
#include <levk/util/not_null.hpp>

namespace levk {
class RenderDevice;
class AssetProviders;
class Scene;

namespace vulkan {
struct SceneRenderer;
} // namespace vulkan

class SceneRenderer {
  public:
	SceneRenderer(NotNull<AssetProviders const*> asset_providers);

	void render(Scene const& scene) const;
	void render(Scene const& scene, RenderList render_list = {}) const;

  private:
	struct Deleter {
		void operator()(vulkan::SceneRenderer const* ptr) const;
	};

	std::unique_ptr<vulkan::SceneRenderer, Deleter> m_impl{};
	NotNull<RenderDevice*> m_render_device;
	NotNull<AssetProviders const*> m_asset_providers;
};
} // namespace levk
