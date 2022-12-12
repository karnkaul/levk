#pragma once
#include <levk/graphics_common.hpp>
#include <span>
#include <type_traits>

namespace levk {
class Texture;

template <typename Type>
concept BufferWrite = (!std::is_pointer_v<Type>) && std::is_trivially_copyable_v<Type>;

struct Shader {
	virtual ~Shader() = default;

	virtual void update(std::uint32_t set, std::uint32_t binding, Texture const& texture) = 0;
	virtual void write(std::uint32_t set, std::uint32_t binding, void const* data, std::size_t size) = 0;

	template <BufferWrite T>
	void write(std::uint32_t set, std::uint32_t binding, T const& t) {
		write(set, binding, &t, sizeof(T));
	}

	template <BufferWrite T>
	void write(std::uint32_t set, std::uint32_t binding, std::span<T> ts) {
		write(set, binding, ts.data(), ts.size_bytes());
	}
};
} // namespace levk
