#include <graphics/vulkan/device.hpp>
#include <levk/graphics/render_device.hpp>
#include <cassert>

namespace levk {
void RenderDevice::Deleter::operator()(vulkan::Device const* ptr) const { delete ptr; }

RenderDevice::RenderDevice(Window const& window, CreateInfo const& create_info)
	: m_vulkan_device(std::unique_ptr<vulkan::Device, Deleter>(new vulkan::Device{window, create_info})) {}

auto RenderDevice::info() const -> Info const& {
	assert(m_vulkan_device);
	return m_vulkan_device->device_info;
}

float RenderDevice::set_render_scale(float desired) {
	assert(m_vulkan_device);
	m_vulkan_device->device_info.render_scale = std::clamp(desired, render_scale_limit_v[0], render_scale_limit_v[1]);
	return m_vulkan_device->device_info.render_scale;
}

std::uint64_t RenderDevice::draw_calls_last_frame() const {
	assert(m_vulkan_device);
	return m_vulkan_device->draw_calls;
}

bool RenderDevice::set_vsync(Vsync desired) {
	assert(m_vulkan_device);
	return m_vulkan_device->set_vsync(desired);
}

void RenderDevice::set_clear(Rgba clear) {
	assert(m_vulkan_device);
	m_vulkan_device->device_info.clear_colour = clear;
}

bool RenderDevice::render(Frame const& frame) {
	assert(m_vulkan_device);
	return m_vulkan_device->render(frame);
}

vulkan::Device& RenderDevice::vulkan_device() const {
	assert(m_vulkan_device);
	return *m_vulkan_device;
}
} // namespace levk
