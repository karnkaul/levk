#pragma once
#include <levk/transform.hpp>
#include <levk/util/nvec3.hpp>
#include <levk/util/radians.hpp>
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
/// \brief Models a 3D camera facing its -Z, with either perspective or orthographic projection parameters.
///
struct Camera {
	enum class Face {
		eNegativeZ,
		ePositiveZ,
	};

	struct Perspective {
		ViewPlane view_plane{0.1f, 1000.0f};
		Radians field_of_view{Degrees{75.0f}};
	};

	struct Orthographic {
		ViewPlane view_plane{-100.0f, 100.0f};
		float view_scale{1.0f};
	};

	static glm::mat4 orthographic(glm::vec2 extent, float view_scale = 1.0f, ViewPlane view_plane = {-100.0f, 100.0f});

	glm::mat4 view() const;
	glm::mat4 projection(glm::vec2 extent) const;

	std::string name{};
	Transform transform{};
	std::variant<Perspective, Orthographic> type{Perspective{}};
	float exposure{2.0f};
	Face face{Face::eNegativeZ};
};
} // namespace levk
