#pragma once
#include <cstdint>

namespace levk {
enum struct Codepoint : std::uint32_t {
	eTofu = 0u,
	eSpace = 32u,
	eAsciiStart = eSpace,
	eAsciiEnd = 126,
};

enum struct TextHeight : std::uint32_t {
	eMin = 8u,
	eDefault = 32u,
	eMax = 256u,
};

constexpr TextHeight clamp(TextHeight in) {
	if (in < TextHeight::eMin) { return TextHeight::eMin; }
	if (in > TextHeight::eMax) { return TextHeight::eMax; }
	return in;
}
} // namespace levk
