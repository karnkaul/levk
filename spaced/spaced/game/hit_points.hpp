#pragma once
#include <algorithm>

namespace spaced {
struct HitPoints {
	float total{100.0f};
	float value{total};

	constexpr float normalized() const { return total <= 0.0f ? 0.0f : value / total; }
	constexpr void modify(float const delta) { value = std::clamp(value + delta, 0.0f, total); }
};
} // namespace spaced
