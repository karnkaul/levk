#include <imgui.h>
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

void Entity::init(Component& out) const { out.m_entity = m_id; }

void Entity::attach(TypeId::value_type type_id, std::unique_ptr<Component>&& out) {
	init(*out);
	if (auto* r = dynamic_cast<RenderComponent*>(out.get())) { m_render_components.insert(r); }
	m_components.insert_or_assign(type_id, std::move(out));
}
} // namespace levk
