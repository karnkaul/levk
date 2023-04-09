#pragma once
#include <graphics/vulkan/pipeline.hpp>
#include <levk/graphics/shader.hpp>
#include <levk/util/not_null.hpp>

namespace levk::vulkan {
struct Shader : levk::Shader {
	vk::Device device{};
	std::span<SetLayout const> set_layouts{};
	std::span<vk::DescriptorSetLayout const> descriptor_set_layouts{};
	std::array<vk::DescriptorSet, max_sets_v> sets{};
	NotNull<SamplerStorage*> sampler_storage;
	NotNull<SetAllocator*> set_allocator;
	NotNull<ScratchBufferAllocator*> scratch_buffer_allocator;

	Shader(DeviceView device, Pipeline const& pipeline)
		: Shader(device.device, pipeline, device.sampler_storage, device.set_allocator, device.scratch_buffer_allocator) {}

	Shader(vk::Device device, Pipeline const& pipeline, NotNull<SamplerStorage*> sampler_storage, NotNull<SetAllocator*> set_allocator,
		   NotNull<ScratchBufferAllocator*> scratch_buffer_allocator)
		: device(device), set_layouts(pipeline.set_layouts), descriptor_set_layouts(pipeline.descriptor_set_layouts), sampler_storage(sampler_storage),
		  set_allocator(set_allocator), scratch_buffer_allocator(scratch_buffer_allocator) {}

	void update(std::uint32_t set, std::uint32_t binding, levk::Texture const& texture) final;
	void write(std::uint32_t set, std::uint32_t binding, void const* data, std::size_t size) final;
	void update(std::uint32_t set, std::uint32_t binding, ShaderBuffer const& buffer) final;
	void update(std::uint32_t set, std::uint32_t binding, ImageView const& image, TextureSampler const& sampler);
	void update(std::uint32_t set, std::uint32_t binding, BufferView const& buffer_view);
	void bind(vk::PipelineLayout layout, vk::CommandBuffer cb) const;
};
} // namespace levk::vulkan
