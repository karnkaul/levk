#pragma once
#include <graphics/vulkan/common.hpp>
#include <levk/util/enumerate.hpp>

namespace levk::vulkan {
template <typename Type>
class BufferVec {
  public:
	static BufferVec make(DeviceView const& device, vk::BufferUsageFlags usage) {
		auto ret = BufferVec{};
		ret.m_buffer = HostBuffer::make(device, usage);
		return ret;
	}

	template <typename F>
	BufferView write(std::size_t size, F for_each) {
		assert(size > 0);
		auto single = Type{};
		auto span = std::span<Type const>{};
		if (size > 1) {
			m_vec.clear();
			m_vec.resize(size);
			for (auto [t, index] : enumerate(m_vec)) { for_each(t, index); }
			span = m_vec;
		} else {
			for_each(single, 0u);
			span = {&single, 1u};
		}
		m_buffer.write(span.data(), span.size_bytes(), span.size());
		return m_buffer.view();
	}

	explicit operator bool() const { return m_buffer.device.device; }

  private:
	HostBuffer m_buffer{};
	std::vector<Type> m_vec{};
};
} // namespace levk::vulkan
