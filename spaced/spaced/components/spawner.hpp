#pragma once
#include <levk/scene/component.hpp>
#include <spaced/game/blueprint.hpp>

namespace spaced {
struct Spawner : levk::Component {
	levk::Ptr<Blueprint> blueprint{};

	std::vector<levk::EntityId> spawned{};

	levk::EntityId spawn();

	void tick(levk::Duration) override;
};
} // namespace spaced
