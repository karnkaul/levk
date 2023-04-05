#pragma once
#include <levk/util/dyn_array.hpp>

namespace levk {
struct ShaderCode {
	DynArray<std::uint32_t> spir_v{};
	std::size_t hash{};
};
} // namespace levk
