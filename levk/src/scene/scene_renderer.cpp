#include <graphics/vulkan/scene_renderer.hpp>
#include <levk/asset/asset_providers.hpp>
#include <levk/graphics/render_device.hpp>
#include <levk/scene/scene.hpp>
#include <levk/scene/scene_renderer.hpp>

namespace levk {
void SceneRenderer::Deleter::operator()(vulkan::SceneRenderer const* ptr) const { delete ptr; }

SceneRenderer::SceneRenderer(NotNull<AssetProviders const*> asset_providers)
	: m_impl(new vulkan::SceneRenderer{asset_providers->render_device().vulkan_device().view()}), m_render_device(&asset_providers->render_device()),
	  m_asset_providers(asset_providers) {}

void SceneRenderer::render(Scene const& scene) const {
	auto render_list = RenderList{};
	render(scene, render_list);
}

void SceneRenderer::render(Scene const& scene, RenderList render_list) const {
	assert(m_impl);
	scene.render(render_list);
	m_impl->build_frame(scene, render_list);
	m_render_device->vulkan_device().render(*m_impl, *m_asset_providers);
}
} // namespace levk