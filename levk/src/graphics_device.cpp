#include <levk/graphics_device.hpp>

namespace levk {
void GraphicsDevice::render(GraphicsRenderer& renderer, Camera const& camera, Lights const& lights, glm::uvec2 extent, Rgba clear) {
	auto render_info = RenderInfo{
		*this, renderer, camera, lights, extent, clear,
	};
	m_model->render(render_info);
}
} // namespace levk
