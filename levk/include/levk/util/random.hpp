#pragma once
#include <cassert>
#include <concepts>
#include <random>

namespace levk::util {
struct Random {
	std::default_random_engine engine{std::random_device{}()};

	template <std::integral T>
	T in_range(T lo, T hi) {
		return std::uniform_int_distribution<T>{lo, hi}(engine);
	}

	template <std::floating_point T>
	T in_range(T lo, T hi) {
		return std::uniform_real_distribution<T>{lo, hi}(engine);
	}

	std::size_t index(std::size_t container_size) {
		assert(container_size > 0u);
		return in_range<std::size_t>(0u, container_size - 1);
	}

	static Random& default_instance() {
		static auto ret = Random{};
		return ret;
	}
};

template <typename T>
T random_range(T lo, T hi) {
	return Random::default_instance().in_range(lo, hi);
}

inline std::size_t random_index(std::size_t container_size) { return Random::default_instance().index(container_size); }
} // namespace levk::util
