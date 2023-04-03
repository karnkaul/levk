#include <graphics/vulkan/device.hpp>
#include <graphics/vulkan/render_camera.hpp>
#include <levk/graphics/render_camera.hpp>

namespace levk {
void RenderCamera::Deleter::operator()(vulkan::RenderCamera const* ptr) const { delete ptr; }

RenderCamera::RenderCamera(vulkan::Device& device, glm::uvec2 target_extent) : m_impl(new vulkan::RenderCamera{vulkan::RenderCamera::make(device.view())}) {
	m_impl->render_target = vulkan::RenderTarget::make_off_screen(m_impl->device, {.extent = {target_extent.x, target_extent.y}});
}

Camera const& RenderCamera::get_camera() const { return m_impl->camera; }

void RenderCamera::set_camera(Camera camera) { m_impl->camera = camera; }

void RenderCamera::set_target_extent(glm::uvec2 extent) {
	m_impl->device.defer->push(std::move(m_impl->render_target));
	m_impl->render_target = vulkan::RenderTarget::make_off_screen(m_impl->device, {.extent = {extent.x, extent.y}});
}
} // namespace levk
