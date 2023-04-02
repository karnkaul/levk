#pragma once
#include <graphics/vulkan/common.hpp>
#include <levk/util/error.hpp>

namespace levk::vulkan {
class DescriptorSet {
  public:
	DescriptorSet(Vma const& vma, SetLayout const& layout, vk::DescriptorSet set, std::uint32_t number)
		: m_layout(layout), m_vma(vma), m_set(set), m_number(number) {}

	void update(std::uint32_t binding, ImageView image, vk::Sampler sampler, vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal) const {
		auto const type = get_type(binding, vk::DescriptorType::eCombinedImageSampler);
		update(vk::DescriptorImageInfo{sampler, image.view, layout}, binding, type);
	}

	void update(std::uint32_t binding, BufferView buffer) const {
		auto const type = get_type(binding, vk::DescriptorType::eUniformBuffer, vk::DescriptorType::eStorageBuffer);
		update(vk::DescriptorBufferInfo{buffer.buffer, buffer.offset, buffer.size}, binding, type);
	}

	void write(std::uint32_t binding, void const* data, std::size_t size) {
		static constexpr auto get_usage = [](vk::DescriptorType const type) {
			switch (type) {
			default:
			case vk::DescriptorType::eUniformBuffer: return vk::BufferUsageFlagBits::eUniformBuffer;
			case vk::DescriptorType::eStorageBuffer: return vk::BufferUsageFlagBits::eStorageBuffer;
			}
		};
		auto const type = get_type(binding, vk::DescriptorType::eUniformBuffer, vk::DescriptorType::eStorageBuffer);
		auto& buffer = m_buffers.emplace_back();
		buffer = m_vma.make_buffer(get_usage(type), size, true);
		std::memcpy(buffer.get().ptr, data, size);
		update(vk::DescriptorBufferInfo{buffer.get().buffer, {}, buffer.get().size}, binding, type);
	}

	vk::DescriptorSet set() const { return m_set; }
	void clear() { m_buffers.clear(); }

	std::uint32_t number() const { return m_number; }

  private:
	template <typename T>
	void update(T const& t, std::uint32_t binding, vk::DescriptorType type) const {
		auto wds = vk::WriteDescriptorSet{m_set, binding, 0, 1, type};
		if constexpr (std::same_as<T, vk::DescriptorBufferInfo>) {
			wds.pBufferInfo = &t;
		} else {
			wds.pImageInfo = &t;
		}
		m_vma.device.updateDescriptorSets(wds, {});
	}

	template <typename... T>
	vk::DescriptorType get_type(std::uint32_t binding, T... match) const noexcept(false) {
		if (binding >= m_layout.bindings.span().size()) { throw Error{fmt::format("DescriptorSet: Invalid binding: {}", binding)}; }
		auto const& bind = m_layout.bindings.span()[binding];
		if ((... && (bind.descriptorType != match))) { throw Error{"DescriptorSet: Invalid descriptor type"}; }
		return bind.descriptorType;
	}

	SetLayout m_layout{};
	Vma m_vma{};
	std::vector<UniqueBuffer> m_buffers{};
	vk::DescriptorSet m_set{};
	std::uint32_t m_number{};
};
} // namespace levk::vulkan
