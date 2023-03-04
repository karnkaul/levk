#pragma once
#include <cstdint>

namespace levk {
enum struct Codepoint : std::uint32_t {
	eNull = 0u,
};

enum struct TextHeight : std::uint32_t {
	eDefault = 32u,
	eMax = 512u,
};

constexpr TextHeight clamp(TextHeight in) { return in > TextHeight::eMax ? TextHeight::eMax : in; }
} // namespace levk
