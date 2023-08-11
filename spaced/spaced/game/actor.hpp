#pragma once
#include <levk/scene/component.hpp>

namespace spaced {
struct Actor : levk::Component {
	levk::EntityId mesh{};

	void tick(levk::Duration) override {}
};
} // namespace spaced
