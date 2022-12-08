#include <levk/util/rgb.hpp>
#include <glm/gtc/color_space.hpp>
#include <glm/mat4x4.hpp>

namespace levk {
glm::vec4 Rgb::to_srgb(glm::vec4 const& linear) { return glm::convertLinearToSRGB(linear); }
glm::vec4 Rgb::to_linear(glm::vec4 const& srgb) { return glm::convertSRGBToLinear(srgb); }
} // namespace levk
