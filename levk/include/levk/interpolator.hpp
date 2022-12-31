#pragma once
#include <levk/transform.hpp>
#include <levk/util/time.hpp>
#include <algorithm>
#include <cassert>
#include <optional>
#include <vector>

namespace levk {
constexpr glm::vec3 lerp(glm::vec3 const& a, glm::vec3 const& b, float const t) {
	return {std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t), std::lerp(a.z, b.z, t)};
}

inline glm::quat lerp(glm::quat const& a, glm::quat const& b, float const t) { return glm::slerp(a, b, t); }

enum class Interpolation {
	eLinear,
	eStep,
};

template <typename T>
struct Interpolator {
	using value_type = T;

	struct Keyframe {
		T value{};
		Time timestamp{};
	};

	std::vector<Keyframe> keyframes{};
	Interpolation interpolation{};

	Time duration() const { return keyframes.empty() ? Time{} : keyframes.back().timestamp; }

	std::optional<std::size_t> index_for(Time time) const {
		if (keyframes.empty()) { return {}; }

		auto const it = std::lower_bound(keyframes.begin(), keyframes.end(), time, [](Keyframe const& k, Time time) { return k.timestamp < time; });
		if (it == keyframes.end()) { return {}; }
		return static_cast<std::size_t>(it - keyframes.begin());
	}

	std::optional<T> operator()(Time elapsed) const {
		if (keyframes.empty()) { return {}; }

		auto const i_next = index_for(elapsed);
		if (!i_next) { return keyframes.back().value; }

		assert(*i_next < keyframes.size());
		auto const& next = keyframes[*i_next];
		assert(elapsed <= next.timestamp);
		if (*i_next == 0) { return next.value; }

		auto const& prev = keyframes[*i_next - 1];
		assert(prev.timestamp < elapsed);
		if (interpolation == Interpolation::eStep) { return prev.value; }

		auto const t = (elapsed - prev.timestamp) / (next.timestamp - prev.timestamp);
		using levk::lerp;
		using std::lerp;
		return lerp(prev.value, next.value, t);
	}
};
} // namespace levk
