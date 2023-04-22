#include <graphics/vulkan/device.hpp>
#include <levk/graphics/render_device.hpp>
#include <cassert>

namespace levk {
namespace {
template <typename Type>
glm::tvec2<Type> clamp_vec(glm::tvec2<Type> in, glm::tvec2<Type> const (&limits)[2]) {
	in.x = std::clamp(in.x, limits[0].x, limits[1].x);
	in.y = std::clamp(in.y, limits[0].y, limits[1].y);
	return in;
}
} // namespace

void RenderDevice::Deleter::operator()(vulkan::Device const* ptr) const { delete ptr; }

RenderDevice::RenderDevice(Window const& window, CreateInfo const& create_info)
	: m_impl(std::unique_ptr<vulkan::Device, Deleter>(new vulkan::Device{window, create_info})) {}

auto RenderDevice::info() const -> Info const& {
	assert(m_impl);
	return m_impl->device_info;
}

float RenderDevice::set_render_scale(float desired) {
	assert(m_impl);
	m_impl->device_info.render_scale = std::clamp(desired, render_scale_limit_v[0], render_scale_limit_v[1]);
	return m_impl->device_info.render_scale;
}

std::uint64_t RenderDevice::draw_calls_last_frame() const {
	assert(m_impl);
	return m_impl->draw_calls();
}

bool RenderDevice::set_vsync(Vsync desired) {
	assert(m_impl);
	return m_impl->set_vsync(desired);
}

void RenderDevice::set_clear(Rgba clear) {
	assert(m_impl);
	m_impl->device_info.clear_colour = clear;
}

void RenderDevice::set_shadow_resolution(Extent2D extent) {
	assert(m_impl);
	m_impl->device_info.shadow_map_resolution = clamp_vec(extent, shadow_resolution_limit_v);
}

vulkan::Device& RenderDevice::vulkan_device() const {
	assert(m_impl);
	return *m_impl;
}
} // namespace levk
