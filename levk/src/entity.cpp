#include <imgui.h>
#include <levk/entity.hpp>
#include <levk/imcpp/reflector.hpp>
#include <levk/scene.hpp>
#include <levk/util/fixed_string.hpp>
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

void Entity::init(Component& out) const {
	out.m_entity = m_id;
	out.m_scene = m_scene;
}

void Entity::attach(TypeId::value_type type_id, std::unique_ptr<Component>&& out) {
	init(*out);
	m_components.insert_or_assign(type_id, std::move(out));
}

void Entity::inspect(imcpp::OpenWindow w) {
	auto* node = scene().node_locator().find(m_node);
	if (!node) { return; }
	imcpp::TreeNode::leaf(node->name.c_str());
	ImGui::Checkbox("Active", &active);
	if (auto tn = imcpp::TreeNode("Transform", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed)) {
		auto unified_scaling = Bool{true};
		imcpp::Reflector{w}(node->transform, unified_scaling, {true});
	}
	auto inspect_component = [w](Component& component) {
		if (auto tn = imcpp::TreeNode{FixedString<128>{component.type_name()}.c_str(), ImGuiTreeNodeFlags_Framed}) { component.inspect(w); }
	};
	for (auto const& [id, component] : m_components) { inspect_component(*component); }
	if (m_renderer) { inspect_component(*m_renderer); }
}

void Entity::set_renderer(std::unique_ptr<Renderer>&& renderer) {
	init(*renderer);
	m_renderer = std::move(renderer);
}
} // namespace levk
