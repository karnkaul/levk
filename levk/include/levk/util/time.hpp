#pragma once
#include <chrono>

namespace levk {
using Clock = std::chrono::steady_clock;
using Secs = std::chrono::duration<float>;

///
/// \brief Stateful delta-time computer.
///
struct DeltaTime {
	///
	/// \brief Time of last update.
	///
	Clock::time_point start{Clock::now()};
	///
	/// \brief Cached value.
	///
	Secs value{};

	///
	/// \brief Update start time and obtain delta time.
	/// \returns Current delta time
	///
	Secs operator()() {
		auto const now = Clock::now();
		value = now - start;
		start = now;
		return value;
	}
};
} // namespace levk
