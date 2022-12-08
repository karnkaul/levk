#pragma once
#include <gltf2cpp/dyn_array.hpp>

namespace levk {
template <typename Type>
using DynArray = gltf2cpp::DynArray<Type>;

using ByteArray = gltf2cpp::ByteArray;
} // namespace levk
