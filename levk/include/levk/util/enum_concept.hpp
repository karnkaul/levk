#pragma once
#include <type_traits>

namespace levk {
template <typename Type>
concept EnumT = std::is_enum_v<Type>;
}
