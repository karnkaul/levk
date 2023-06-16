#pragma once
#include <levk/util/enum_concept.hpp>
#include <array>

namespace levk {
template <EnumT E, std::size_t Capacity>
struct AxisState {
	std::array<float, Capacity> state{};

	constexpr float value(E e) const {
		auto const index = static_cast<std::size_t>(e);
		if (index > state.size()) { return {}; }
		return state[static_cast<std::size_t>(e)];
	}
};
} // namespace levk
