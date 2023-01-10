#include <levk/graphics_device.hpp>

namespace levk {
void GraphicsDevice::render(StaticMesh const& mesh, RenderResources const& resources, std::span<Transform const> instances, glm::mat4 const& parent) {
	auto const smri = StaticMeshRenderInfo{
		resources,
		mesh,
		parent,
		instances,
	};
	m_model->render(smri);
}

void GraphicsDevice::render(refactor::StaticMesh const& mesh, refactor::RenderResources const& resources, std::span<Transform const> instances,
							glm::mat4 const& parent) {
	auto const smri = refactor::StaticMeshRenderInfo{
		resources,
		mesh,
		parent,
		instances,
	};
	m_model->render(smri);
}

void GraphicsDevice::render(SkinnedMesh const& mesh, RenderResources const& resources, std::span<glm::mat4 const> joints) {
	auto const skri = SkinnedMeshRenderInfo{
		resources,
		mesh,
		joints,
	};
	m_model->render(skri);
}

void GraphicsDevice::render(refactor::SkinnedMesh const& mesh, refactor::RenderResources const& resources, std::span<glm::mat4 const> joints) {
	auto const skri = refactor::SkinnedMeshRenderInfo{
		resources,
		mesh,
		joints,
	};
	m_model->render(skri);
}

void GraphicsDevice::render(GraphicsRenderer& renderer, Camera const& camera, Lights const& lights, glm::uvec2 extent, Rgba clear) {
	auto const render_info = RenderInfo{renderer, camera, lights, extent, clear, default_render_mode};
	m_model->render(render_info);
}
} // namespace levk
