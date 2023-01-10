#include <imgui.h>
#include <levk/imcpp/reflector.hpp>
#include <levk/imcpp/scene_inspector.hpp>
#include <levk/util/fixed_string.hpp>

namespace levk::imcpp {
bool SceneInspector::check_stale() {
	bool ret = false;
	if (m_scene != m_prev) {
		m_prev = m_scene;
		m_inspecting = {};
		ret = true;
	}
	if (m_inspecting.type == Inspect::Type::eEntity && !m_scene->find(m_inspecting.entity)) { m_inspecting = {}; }
	return ret;
}

SceneInspector::Inspect SceneInspector::inspect(NotClosed<Window> w, Scene& scene) {
	m_scene = &scene;
	check_stale();

	camera_node();
	draw_scene_tree(w);
	if (auto* payload = ImGui::GetDragDropPayload(); payload && payload->IsDataType("node")) {
		imcpp::TreeNode::leaf("(Unparent)");
		if (auto target = imcpp::DragDrop::Target{}) {
			if (auto const* node_id = imcpp::DragDrop::accept<std::size_t>("node")) {
				auto node_locator = scene.node_locator();
				node_locator.reparent(node_locator.get(*node_id), {});
			}
		}
	}

	if (bool show_inspector = static_cast<bool>(m_inspecting)) {
		ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x - 500.0f, 0.0f});
		ImGui::SetNextWindowSize({500.0f, ImGui::GetIO().DisplaySize.y});
		if (auto w = imcpp::Window{"Inspector", &show_inspector}) { draw_inspector(w); }
		if (!show_inspector) { m_inspecting = {}; }
	}

	return m_inspecting;
}

void SceneInspector::camera_node() {
	auto flags = int{};
	flags |= (ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow);
	if (m_inspecting.type == Inspect::Type::eCamera) { flags |= ImGuiTreeNodeFlags_Selected; }
	if (imcpp::TreeNode::leaf("Camera", flags)) { m_inspecting.type = Inspect::Type::eCamera; }
}

bool SceneInspector::walk_node(Node& node) {
	auto node_locator = m_scene->node_locator();
	auto flags = int{};
	flags |= (ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow);
	if (node.entity && m_inspecting == node.entity) { flags |= ImGuiTreeNodeFlags_Selected; }
	if (node.children().empty()) { flags |= ImGuiTreeNodeFlags_Leaf; }
	auto tn = imcpp::TreeNode{node.name.c_str(), flags};
	if (ImGui::IsItemClicked() && node.entity) { m_inspecting = {node.entity, Inspect::Type::eEntity}; }
	auto const id = node.id().value();
	if (auto source = imcpp::DragDrop::Source{}) { imcpp::DragDrop::set<std::size_t>("node", id, node.name); }
	if (auto target = imcpp::DragDrop::Target{}) {
		if (auto const* node_id = imcpp::DragDrop::accept<std::size_t>("node")) {
			node_locator.reparent(node_locator.get(*node_id), id);
			return false;
		}
	}
	if (tn) {
		for (auto& id : node.children()) {
			if (!walk_node(node_locator.get(id))) { return false; }
		}
	}
	return true;
}

void SceneInspector::draw_scene_tree(imcpp::OpenWindow) {
	auto node_locator = m_scene->node_locator();
	for (auto const& node : node_locator.roots()) {
		if (!walk_node(node_locator.get(node))) { return; }
	}
}

void SceneInspector::draw_inspector(imcpp::OpenWindow w) {
	assert(m_scene);
	switch (m_inspecting.type) {
	case Inspect::Type::eCamera: {
		imcpp::TreeNode::leaf("Camera", ImGuiTreeNodeFlags_SpanFullWidth);
		if (auto tn = imcpp::TreeNode{"Transform", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen}) {
			Bool unified_scaling{true};
			imcpp::Reflector{w}(m_scene->camera.transform, unified_scaling, {});
		}
		break;
	}
	case Inspect::Type::eLights: {
		break;
	}
	default: {
		auto* entity = m_scene->find(m_inspecting.entity);
		if (!entity) { return; }
		entity->inspect(w);
		break;
	}
	}
}
} // namespace levk::imcpp
