#pragma once
#include <glm/gtx/quaternion.hpp>
#include <levk/graphics/rgba.hpp>
#include <levk/util/nvec3.hpp>
#include <vector>

namespace levk {
///
/// \brief Directional light.
///
struct DirLight {
	static constexpr std::uint32_t binding_v{0u};

	///
	/// \brief Direction.
	///
	glm::quat direction{glm::angleAxis(glm::radians(180.0f), up_v)};
	///
	/// \brief Colour and intensity.
	///
	/// Alpha is ignored.
	///
	HdrRgba rgb{white_v, 5.0f};
};

struct Lights {
	static constexpr std::uint32_t set_v{1u};

	DirLight primary{};
	std::vector<DirLight> dir_lights{};
};
} // namespace levk
