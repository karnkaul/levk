#pragma once
#include <glm/gtx/quaternion.hpp>
#include <levk/rgba.hpp>
#include <levk/util/nvec3.hpp>
#include <vector>

namespace levk {
///
/// \brief Directional light.
///
struct DirLight {
	///
	/// \brief Direction.
	///
	glm::quat direction{glm::angleAxis(glm::radians(180.0f), up_v)};
	///
	/// \brief Colour and intensity.
	///
	/// Alpha is ignored.
	///
	HdrRgba rgb{5.0f};
};

struct Lights {
	std::vector<DirLight> dir_lights{DirLight{}};
};
} // namespace levk
