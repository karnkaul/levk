#include <graphics/vulkan/shader.hpp>
#include <graphics/vulkan/texture.hpp>
#include <levk/graphics/shader_buffer.hpp>
#include <levk/graphics/texture.hpp>
#include <levk/util/enumerate.hpp>

namespace levk::vulkan {
void Shader::update(std::uint32_t set, std::uint32_t binding, levk::Texture const& texture) {
	auto* vtex = texture.vulkan_texture();
	if (!vtex) { return; }
	return update(set, binding, vtex->image.get().get().image_view(), texture.sampler);
}

void Shader::write(std::uint32_t set, std::uint32_t binding, void const* data, std::size_t size) {
	if (set >= sets.size() || set >= descriptor_set_layouts.size()) { return; }
	if (binding >= set_layouts[set].bindings.size()) { return; }
	auto& layout_binding = set_layouts[set].bindings.span()[binding];
	auto const usage = [layout_binding] {
		switch (layout_binding.descriptorType) {
		case vk::DescriptorType::eStorageBuffer: return vk::BufferUsageFlagBits::eStorageBuffer;
		default: return vk::BufferUsageFlagBits::eUniformBuffer;
		}
	}();
	auto& descriptor_set = sets[set];
	if (!descriptor_set) { descriptor_set = set_allocator->allocate(descriptor_set_layouts[set]); }
	auto& buffer = scratch_buffer_allocator->allocate(size, usage);
	std::memcpy(buffer.mapped, data, size);
	auto wds = vk::WriteDescriptorSet{descriptor_set};
	auto const buffer_info = vk::DescriptorBufferInfo{buffer.buffer, 0u, size};
	wds.descriptorCount = layout_binding.descriptorCount;
	wds.descriptorType = layout_binding.descriptorType;
	wds.dstBinding = binding;
	wds.pBufferInfo = &buffer_info;
	device.updateDescriptorSets(wds, {});
}

void Shader::update(std::uint32_t set, std::uint32_t binding, ShaderBuffer const& buffer) {
	if (set >= sets.size()) { return; }
	auto* host_buffer = buffer.vulkan_buffer();
	if (!host_buffer) { return; }
	update(set, binding, host_buffer->view());
}

void Shader::update(std::uint32_t set, std::uint32_t binding, ImageView const& image, TextureSampler const& sampler) {
	if (set >= sets.size() || set >= descriptor_set_layouts.size()) { return; }
	if (binding >= set_layouts[set].bindings.size()) { return; }
	auto& descriptor_set = sets[set];
	if (!descriptor_set) { descriptor_set = set_allocator->allocate(descriptor_set_layouts[set]); }
	auto wds = vk::WriteDescriptorSet{descriptor_set};
	auto const vk_sampler = sampler_storage->get(device, sampler);
	if (!vk_sampler) { return; }
	auto const image_info = vk::DescriptorImageInfo{vk_sampler, image.view, vk::ImageLayout::eReadOnlyOptimal};
	wds.descriptorCount = 1u;
	wds.descriptorType = vk::DescriptorType::eCombinedImageSampler;
	wds.dstBinding = binding;
	wds.pImageInfo = &image_info;
	device.updateDescriptorSets(wds, {});
}

void Shader::update(std::uint32_t set, std::uint32_t binding, BufferView const& buffer_view) {
	if (!buffer_view.buffer) { return; }
	auto& descriptor_set = sets[set];
	if (!descriptor_set) { descriptor_set = set_allocator->allocate(descriptor_set_layouts[set]); }
	if (binding >= set_layouts[set].bindings.size()) { return; }
	auto& layout_binding = set_layouts[set].bindings.span()[binding];
	auto wds = vk::WriteDescriptorSet{descriptor_set};
	auto const buffer_info = vk::DescriptorBufferInfo{buffer_view.buffer, buffer_view.offset, buffer_view.size};
	wds.descriptorCount = layout_binding.descriptorCount;
	wds.descriptorType = layout_binding.descriptorType;
	wds.dstBinding = binding;
	wds.pBufferInfo = &buffer_info;
	device.updateDescriptorSets(wds, {});
}

void Shader::bind(vk::PipelineLayout layout, vk::CommandBuffer cb) const {
	for (auto const& [descriptor_set, number] : enumerate<std::uint32_t>(sets)) {
		if (!descriptor_set) { continue; }
		cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, number, descriptor_set, {});
	}
}
} // namespace levk::vulkan
