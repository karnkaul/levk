#pragma once
#include <cstdint>

namespace levk {
///
/// \brief Strongly typed alias for booleans (no implicit conversions)
///
struct Bool {
	bool value{};

	explicit constexpr operator bool() const { return value; }
};
} // namespace levk
