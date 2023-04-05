#pragma once
#include <levk/util/enum_concept.hpp>
#include <cstdint>

namespace levk {
///
/// \brief Stores an array of Type, of size Size, indexable by enum E.
///
template <EnumT E, typename Type, std::size_t Size = static_cast<std::size_t>(E::eCOUNT_)>
struct EnumArray {
	Type t[Size]{};

	constexpr Type& at(E const e) { return t[static_cast<std::size_t>(e)]; }
	constexpr Type const& at(E const e) const { return t[static_cast<std::size_t>(e)]; }

	constexpr Type& operator[](E const e) { return t[static_cast<std::size_t>(e)]; }
	constexpr Type const& operator[](E const e) const { return t[static_cast<std::size_t>(e)]; }
};
} // namespace levk
