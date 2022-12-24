#pragma once
#include <levk/entity.hpp>
#include <levk/node.hpp>
#include <levk/util/monotonic_map.hpp>

namespace levk {
struct SceneTree {
	Entity::Map entities{};
	Node::Tree nodes{};

	Node& add(Entity entity, Node::CreateInfo const& node_create_info = {}) {
		auto& ret = nodes.add(node_create_info);
		ret.entity = entities.add(std::move(entity)).first;
		return ret;
	}
};
} // namespace levk
