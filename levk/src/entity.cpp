#include <levk/entity.hpp>
#include <algorithm>

namespace levk {
void Entity::tick(Time dt) {
	m_sorted.clear();
	m_sorted.reserve(m_components.size());
	for (auto const& [_, component] : m_components) { m_sorted.push_back(component.get()); }
	std::sort(m_sorted.begin(), m_sorted.end(), [](auto const* a, auto const* b) { return a->id() < b->id(); });
	for (auto* component : m_sorted) { component->tick(dt); }
}

Scene& Entity::scene() const {
	assert(m_scene);
	return *m_scene;
}

void Entity::attach(TypeId::value_type type_id, std::unique_ptr<Component>&& out) {
	out->m_entity = m_id;
	out->m_scene = m_scene;
	m_components.insert_or_assign(type_id, std::move(out));
}
} // namespace levk
