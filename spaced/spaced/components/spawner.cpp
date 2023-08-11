#include <levk/scene/scene.hpp>
#include <spaced/components/spawner.hpp>
#include <algorithm>

namespace spaced {
levk::EntityId Spawner::spawn() {
	assert(blueprint);
	auto& ret = owning_scene()->spawn({.parent = owning_entity()->node_id()});
	blueprint->create(ret);
	spawned.push_back(ret.id());
	return ret.id();
}

void Spawner::tick(levk::Duration) {
	std::erase_if(spawned, [this](levk::EntityId const id) { return !owning_scene()->find_entity(id); });
}
} // namespace spaced
