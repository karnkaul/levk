#pragma once
#include <levk/util/dyn_array.hpp>
#include <array>
#include <span>
#include <vector>

namespace levk {
template <typename Type, std::size_t Size>
struct ArrayStorage {
	using value_type = Type;
	using container_type = std::array<Type, Size>;

	container_type data{};

	std::span<Type> span() { return data; }
	std::span<Type const> span() const { return data; }
};

template <typename Type>
struct DynArrayStorage {
	using value_type = Type;
	using container_type = DynArray<Type>;

	container_type data{};

	std::span<Type> span() { return data; }
	std::span<Type const> span() const { return data; }
};

template <typename Type>
struct VectorStorage {
	using value_type = Type;
	using container_type = std::vector<Type>;

	container_type data{};

	std::span<Type> span() { return data; }
	std::span<Type const> span() const { return data; }
};
} // namespace levk
