#pragma once
#include <levk/util/scale_extent.hpp>

namespace spaced::layout {
inline constexpr glm::uvec2 extent_v{1280u, 720u};
inline constexpr glm::vec2 game_view_v{levk::ScaleExtent{extent_v}.fixed_width(20.0f)};
} // namespace spaced::layout
