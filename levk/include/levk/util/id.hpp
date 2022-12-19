#pragma once
#include <concepts>
#include <cstdint>

namespace levk {
///
/// \brief Represents a strongly-typed integral ID.
///
template <typename Type, std::equality_comparable Value = std::size_t>
class Id {
  public:
	using id_type = Value;

	///
	/// \brief Implicit constructor
	/// \param value The underlying value of this instance
	///
	constexpr Id(Value value = {}) : m_value(static_cast<Value&&>(value)) {}

	constexpr Value const& value() const { return m_value; }
	constexpr operator Value const&() const { return value(); }

	bool operator==(Id const&) const = default;

  private:
	Value m_value{};
};
} // namespace levk
