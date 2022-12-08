#pragma once
#include <concepts>
#include <cstdint>

namespace levk {
///
/// \brief Concept wrapping is_enum_v trait.
///
template <typename Type>
concept TEnum = std::is_enum_v<Type>;

///
/// \brief Stores an array of Type, of size Size, indexable by enum E.
///
template <TEnum E, typename Type, std::size_t Size = static_cast<std::size_t>(E::eCOUNT_)>
struct EnumArray {
	Type t[Size]{};

	constexpr Type& at(E const e) { return t[static_cast<std::size_t>(e)]; }
	constexpr Type const& at(E const e) const { return t[static_cast<std::size_t>(e)]; }

	constexpr Type& operator[](E const e) { return t[static_cast<std::size_t>(e)]; }
	constexpr Type const& operator[](E const e) const { return t[static_cast<std::size_t>(e)]; }
};
} // namespace levk
