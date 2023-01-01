#pragma once
#include <levk/entity.hpp>
#include <levk/node.hpp>
#include <levk/util/monotonic_map.hpp>
#include <levk/util/time.hpp>

namespace levk::experiment {
struct SceneTree {
	Entity::Map entities{};
	Node::Tree nodes{};

	std::vector<Ptr<Entity>> sorted{};

	Node& spawn(Entity entity, Node::CreateInfo const& node_create_info = {});
	void destroy(Id<Entity> id);

	void tick(Time dt);
};
} // namespace levk::experiment
