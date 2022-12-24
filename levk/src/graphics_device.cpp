#include <levk/graphics_device.hpp>

namespace levk {
void GraphicsDevice::render(Mesh const& mesh, MeshResources const& resources, std::span<Transform const> instances, glm::mat4 const& parent) {
	auto const smri = StaticMeshRenderInfo{
		resources,
		mesh,
		parent,
		instances,
	};
	m_model->render(smri);
}

void GraphicsDevice::render(Mesh const& mesh, MeshResources const& resources, Skeleton::Instance const& skeleton, Node::Tree const& tree) {
	auto const skri = SkinnedMeshRenderInfo{
		resources,
		mesh,
		skeleton,
		tree,
	};
	m_model->render(skri);
}

void GraphicsDevice::render(GraphicsRenderer& renderer, Camera const& camera, Lights const& lights, glm::uvec2 extent, Rgba clear) {
	auto const render_info = RenderInfo{renderer, camera, lights, extent, clear};
	m_model->render(render_info);
}
} // namespace levk
