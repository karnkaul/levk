#pragma once
#include <glm/vec2.hpp>
#include <variant>

namespace levk {
namespace event {
struct Close {};

struct Resize {
	enum class Type { eFramebuffer, eWindow };

	glm::uvec2 extent{};
	Type type{};
};
} // namespace event

using Event = std::variant<event::Close, event::Resize>;
} // namespace levk
