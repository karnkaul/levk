#include <experiment/scene_tree.hpp>
#include <algorithm>

namespace levk::experiment {
Node& SceneTree::spawn(Entity entity, Node::CreateInfo const& node_create_info) {
	auto& ret = nodes.add(node_create_info);
	ret.entity = entities.add(std::move(entity)).first;
	return ret;
}

void SceneTree::tick(Time dt) {
	sorted.clear();
	sorted.reserve(entities.size());
	auto populate = [&](Id<Entity>, Entity& value) { sorted.push_back(&value); };
	entities.for_each(populate);
	std::sort(sorted.begin(), sorted.end(), [](Ptr<Entity> a, Ptr<Entity> b) { return a->id() < b->id(); });
}
} // namespace levk::experiment
