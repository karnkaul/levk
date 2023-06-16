#pragma once
#include <levk/util/enum_concept.hpp>
#include <levk/window/common.hpp>
#include <array>

namespace levk {
template <EnumT E, std::size_t Capacity>
struct ActionState {
	std::array<Action, Capacity> state{};

	template <EnumT T>
	constexpr Action action(T const t) const {
		auto const index = static_cast<std::size_t>(t);
		if (index >= state.size()) { return Action::eNone; }
		return state[index];
	}

	template <std::same_as<E>... T>
	constexpr bool is_pressed(T const... t) const {
		return ((action(t) == Action::ePress) || ...);
	}

	template <std::same_as<E>... T>
	constexpr bool is_held(T const... t) const {
		auto held = [this](E const e) {
			auto const a = action(e);
			return a == Action::eHold || a == Action::eRepeat;
		};
		return ((held(t)) || ...);
	}

	template <std::same_as<E>... T>
	constexpr bool is_repeated(T const... t) const {
		return ((action(t) == Action::eRepeat) || ...);
	}

	template <std::same_as<E>... T>
	constexpr bool is_released(T const... t) const {
		return ((action(t) == Action::eRelease) || ...);
	}

	template <std::same_as<E>... T>
	constexpr bool chord(E const press, T const... hold) const {
		return is_pressed(press) && (is_held(hold) && ...);
	}
};
} // namespace levk
