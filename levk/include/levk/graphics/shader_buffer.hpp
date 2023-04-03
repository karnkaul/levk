#pragma once
#include <levk/util/ptr.hpp>
#include <memory>

namespace levk {
namespace vulkan {
struct Device;
struct HostBuffer;
} // namespace vulkan

class ShaderBuffer {
  public:
	virtual ~ShaderBuffer() = default;

	void write(void const* data, std::size_t size);

	Ptr<vulkan::HostBuffer> vulkan_buffer() const { return m_impl.get(); }

  protected:
	ShaderBuffer() = default;

	struct Deleter {
		void operator()(vulkan::HostBuffer const* ptr) const;
	};

	std::unique_ptr<vulkan::HostBuffer, Deleter> m_impl{};
};

class UniformBuffer : public ShaderBuffer {
  public:
	UniformBuffer(vulkan::Device& device);
};

class StorageBuffer : public ShaderBuffer {
  public:
	StorageBuffer(vulkan::Device& device);
};
} // namespace levk
