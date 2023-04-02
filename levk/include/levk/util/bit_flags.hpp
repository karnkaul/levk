#pragma once
#include <levk/util/enum_concept.hpp>
#include <concepts>
#include <cstdint>

namespace levk {
template <EnumT E, typename Type = std::underlying_type_t<E>>
class BitFlags {
  public:
	using enum_type = E;

	BitFlags() = default;

	template <std::same_as<E>... T>
	constexpr BitFlags(T const... t) {
		(set(t), ...);
	}

	constexpr BitFlags& set(E const e) {
		m_bits |= static_cast<Type>(e);
		return *this;
	}

	constexpr BitFlags& reset(E const e) {
		m_bits &= ~static_cast<Type>(e);
		return *this;
	}

	constexpr bool test(E const e) const { return (m_bits & static_cast<Type>(e)) == static_cast<Type>(e); }

	constexpr bool any(BitFlags const& rhs) const { return (m_bits & rhs.m_bits) != Type{}; }
	constexpr bool all(BitFlags const& rhs) const { return (m_bits & rhs.m_bits) == rhs.m_bits; }
	constexpr bool none(BitFlags const& rhs) const { return !any(rhs); }

	bool operator==(BitFlags const&) const = default;

  private:
	Type m_bits{};
};
} // namespace levk
