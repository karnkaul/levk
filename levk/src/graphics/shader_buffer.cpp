#include <graphics/vulkan/common.hpp>
#include <graphics/vulkan/device.hpp>
#include <levk/graphics/shader_buffer.hpp>

namespace levk {
void ShaderBuffer::Deleter::operator()(vulkan::HostBuffer const* ptr) const { delete ptr; }

void ShaderBuffer::write(void const* data, std::size_t size) {
	assert(m_impl);
	m_impl->write(data, size);
}

UniformBuffer::UniformBuffer(vulkan::Device& device) {
	m_impl = std::unique_ptr<vulkan::HostBuffer, Deleter>{new vulkan::HostBuffer{}};
	*m_impl = vulkan::HostBuffer::make(device.view(), vk::BufferUsageFlagBits::eUniformBuffer);
}

StorageBuffer::StorageBuffer(vulkan::Device& device) {
	m_impl = std::unique_ptr<vulkan::HostBuffer, Deleter>{new vulkan::HostBuffer{}};
	*m_impl = vulkan::HostBuffer::make(device.view(), vk::BufferUsageFlagBits::eStorageBuffer);
}
} // namespace levk
