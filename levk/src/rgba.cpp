#include <glm/gtc/color_space.hpp>
#include <glm/mat4x4.hpp>
#include <levk/rgba.hpp>

namespace levk {
glm::vec4 Rgba::to_srgb(glm::vec4 const& linear) { return glm::convertLinearToSRGB(linear); }
glm::vec4 Rgba::to_linear(glm::vec4 const& srgb) { return glm::convertSRGBToLinear(srgb); }
} // namespace levk
