#include <levk/graphics_device.hpp>
#include <levk/impl/vulkan_device.hpp>

namespace levk {
void GraphicsDevice::render(StaticMesh const& mesh, AssetProviders const& providers, std::span<Transform const> instances, glm::mat4 const& parent) {
	auto const smri = StaticMeshRenderInfo{
		providers,
		mesh,
		parent,
		instances,
	};
	m_model->render(smri);
}

void GraphicsDevice::render(SkinnedMesh const& mesh, AssetProviders const& providers, std::span<glm::mat4 const> joints) {
	auto const skri = SkinnedMeshRenderInfo{
		providers,
		mesh,
		joints,
	};
	m_model->render(skri);
}

void GraphicsDevice::render(GraphicsRenderer const& renderer, AssetProviders const& providers, Camera const& camera, Lights const& lights, glm::uvec2 extent,
							Rgba clear) {
	auto const render_info = RenderInfo{renderer, providers, camera, lights, extent, clear, default_render_mode};
	m_model->render(render_info);
}

GraphicsDevice VulkanDeviceFactory::make() const { return VulkanDevice{}; }
} // namespace levk
