#pragma once
#include <levk/transform.hpp>
#include <levk/util/nvec3.hpp>
#include <string>
#include <variant>

namespace levk {
///
/// \brief The view / z plane for Camera.
///
struct ViewPlane {
	float near{};
	float far{};
};

///
/// \brief Models a 3D camera with either perspective or orthographic projection parameters.
///
/// The view matrix is modelled through a Transform that's passed in as a parameter.
///
struct Camera {
	struct Perspective {
		ViewPlane view_plane{0.1f, 1000.0f};
		float field_of_view{glm::radians(45.0f)};
	};

	struct Orthographic {
		ViewPlane view_plane{-100.0f, 100.0f};
	};

	glm::mat4x4 view() const;
	glm::mat4x4 projection(glm::vec2 extent) const;

	std::string name{};
	Transform transform{};
	std::variant<Perspective, Orthographic> type{Perspective{}};
	float exposure{2.0f};
};
} // namespace levk
