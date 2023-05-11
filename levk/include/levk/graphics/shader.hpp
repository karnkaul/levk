#pragma once
#include <levk/graphics/common.hpp>
#include <span>
#include <type_traits>

namespace levk {
namespace vulkan {
class Texture;
}
class ShaderBuffer;

template <typename Type>
concept BufferWriteT = (!std::is_pointer_v<Type>)&&std::is_trivially_copyable_v<Type>;

struct Shader {
	virtual ~Shader() = default;

	virtual void update(std::uint32_t set, std::uint32_t binding, vulkan::Texture const& texture) = 0;
	virtual void update(std::uint32_t set, std::uint32_t binding, ShaderBuffer const& buffer) = 0;
	virtual void write(std::uint32_t set, std::uint32_t binding, void const* data, std::size_t size) = 0;

	template <BufferWriteT T>
	void write(std::uint32_t set, std::uint32_t binding, T const& t) {
		write(set, binding, &t, sizeof(T));
	}

	template <BufferWriteT T>
	void write(std::uint32_t set, std::uint32_t binding, std::span<T> ts) {
		write(set, binding, ts.data(), ts.size_bytes());
	}
};
} // namespace levk
