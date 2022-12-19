#include <fmt/format.h>
#include <levk/entity.hpp>
#include <levk/util/error.hpp>
#include <algorithm>
#include <utility>

namespace levk {
Entity& Entity::Tree::add(Entity entity, Id<Entity> parent) {
	entity.m_id = ++m_next_id;
	assert(entity.m_id != parent);
	if (parent) {
		if (auto it = m_entities.find(parent); it != m_entities.end()) {
			it->second.m_children.push_back(entity.m_id);
			entity.m_parent = parent;
		}
	}
	if (!parent) { m_roots.push_back(entity.m_id); }
	auto [it, _] = m_entities.insert_or_assign(entity.m_id, std::move(entity));
	return it->second;
}

void Entity::Tree::remove(Id<Entity> id) {
	if (auto it = m_entities.find(id); it != m_entities.end()) {
		set_parent_on_children(it->second, {});
		remove_child_from_parent(it->second);
		m_entities.erase(it);
		std::erase(m_roots, id);
	}
}

void Entity::Tree::reparent(Entity& out, Id<Entity> new_parent) {
	assert(out.m_id != new_parent);
	if (!out.m_parent && new_parent) { std::erase(m_roots, out.m_id); }
	if (out.m_parent && !new_parent) { m_roots.push_back(out.m_id); }
	set_parent_on_children(out, new_parent);
	remove_child_from_parent(out);
	if (auto* parent = find(new_parent)) { parent->m_children.push_back(out.m_id); }
	out.m_parent = new_parent;
}

void Entity::Tree::set_parent_on_children(Entity& out, Id<Entity> parent) {
	for (auto& child : out.m_children) {
		if (auto* entity = find(child)) { entity->m_parent = parent; }
	}
}

void Entity::Tree::remove_child_from_parent(Entity& out) {
	if (auto* parent = find(out.m_parent)) { std::erase(parent->m_children, out.m_id); }
}

Ptr<Entity const> Entity::Tree::find(Id<Entity> id) const {
	if (id == Id<Entity>{}) { return {}; }
	if (auto it = m_entities.find(id); it != m_entities.end()) { return &it->second; }
	return {};
}

Ptr<Entity> Entity::Tree::find(Id<Entity> id) { return const_cast<Entity*>(std::as_const(*this).find(id)); }

Entity const& Entity::Tree::get(Id<Entity> id) const {
	auto const* ret = find(id);
	if (!ret) { throw Error{fmt::format("Invalid entity id: {}", id)}; }
	return *ret;
}

Entity& Entity::Tree::get(Id<Entity> id) { return const_cast<Entity&>(std::as_const(*this).get(id)); }
} // namespace levk
