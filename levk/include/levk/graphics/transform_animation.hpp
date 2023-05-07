#pragma once
#include <levk/interpolator.hpp>
#include <levk/transform.hpp>
#include <variant>

namespace levk {
struct TransformAnimation {
	struct Translate : Interpolator<glm::vec3> {};
	struct Rotate : Interpolator<glm::quat> {};
	struct Scale : Interpolator<glm::vec3> {};

	struct Sampler {
		std::variant<Translate, Rotate, Scale> storage{};

		void update(Transform& out, Duration time) const;
		Duration duration() const;
	};

	std::vector<Sampler> samplers{};

	Duration duration() const;
};
} // namespace levk
