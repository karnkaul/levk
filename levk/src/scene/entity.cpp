#include <levk/scene/entity.hpp>
#include <levk/scene/scene.hpp>
#include <algorithm>
#include <ranges>

namespace levk {
Transform& Entity::transform() {
	assert(m_scene);
	return m_scene->node_locator().get(node_id()).transform;
}

void Entity::tick(Duration dt) {
	auto const to_tick = sorted_components();
	std::ranges::for_each(to_tick, [dt](Ptr<Component> component) { component->tick(dt); });

	for (auto const to_detach : m_to_detach) {
		auto it = m_components.find(to_detach.value());
		if (it == m_components.end()) { continue; }
		m_render_components.erase(it->second.get());
		m_components.erase(it);
	}
	m_to_detach.clear();
}

void Entity::render(DrawList& out) const {
	for (auto const& [_, component] : m_render_components) { component->render(out); }
}

void Entity::attach(TypeId::value_type type_id, std::unique_ptr<Component>&& out, bool is_render) {
	auto* component = out.get();
	m_components.insert_or_assign(type_id, std::move(out));

	component->m_id = ++m_prev_id;
	component->m_entity = m_id;
	component->m_scene = m_scene;
	if (is_render) { m_render_components.insert_or_assign(component, static_cast<RenderComponent*>(component)); }

	component->setup();
}

std::vector<Ptr<Component>> Entity::sorted_components() const {
	auto ret = std::vector<Ptr<Component>>{};
	ret.reserve(m_components.size());
	for (auto const& [_, component] : m_components) { ret.push_back(component.get()); }
	std::ranges::sort(ret, [](auto const& a, auto const& b) { return a->id() < b->id(); });
	return ret;
}
} // namespace levk
