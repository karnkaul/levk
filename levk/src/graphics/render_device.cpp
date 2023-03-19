#include <levk/graphics/render_device.hpp>
#include <levk/impl/vulkan_device.hpp>

namespace levk {
void RenderDevice::render(Renderer const& renderer, AssetProviders const& ap, Camera const& camera, Lights const& lights, glm::uvec2 fb_extent) {
	auto const render_info = RenderInfo{renderer, ap, camera, lights, fb_extent, clear_colour, default_render_mode};
	m_model->render(render_info);
}

RenderDevice VulkanDeviceFactory::make() const { return VulkanDevice{}; }
} // namespace levk
