#include <experiment/entity.hpp>
#include <algorithm>

namespace levk::experiment {
void Entity::tick(Time dt) {
	m_sorted.tick.clear();
	m_sorted.tick.reserve(m_components.tick.size());
	for (auto const& [_, component] : m_components.tick) { m_sorted.tick.push_back(component); }
	std::sort(m_sorted.tick.begin(), m_sorted.tick.end(), [](auto const* a, auto const* b) { return a->id() < b->id(); });
	for (auto* component : m_sorted.tick) { component->tick(dt); }
}

Scene& Entity::scene() const {
	assert(m_scene);
	return *m_scene;
}
} // namespace levk::experiment
