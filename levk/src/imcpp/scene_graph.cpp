#include <imgui.h>
#include <levk/imcpp/reflector.hpp>
#include <levk/imcpp/scene_graph.hpp>
#include <levk/serializer.hpp>
#include <levk/service.hpp>
#include <levk/util/fixed_string.hpp>

namespace levk::imcpp {
bool SceneGraph::check_stale() {
	bool ret = false;
	if (m_scene != m_prev) {
		m_prev = m_scene;
		m_inspecting = {};
		ret = true;
	}
	if (m_inspecting.type == Inspect::Type::eEntity && !m_scene->find(m_inspecting.entity)) { m_inspecting = {}; }
	return ret;
}

SceneGraph::Inspect SceneGraph::draw_to(NotClosed<Window> w, Scene& scene) {
	m_scene = &scene;
	check_stale();

	if (ImGui::SliderFloat("Inspector Width", &m_inspector_nwidth, 0.1f, 0.5f, "%.3f")) { m_inspector_nwidth = std::clamp(m_inspector_nwidth, 0.1f, 0.5f); }

	ImGui::Separator();
	if (ImGui::Button("Spawn")) { Popup::open("scene_inspector.spawn_entity"); }

	ImGui::Separator();
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
		auto const width = ImGui::GetMainViewport()->Size.x * m_inspector_nwidth;
		ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x - width, 0.0f});
		ImGui::SetNextWindowSize({width, ImGui::GetIO().DisplaySize.y});
		if (auto w = imcpp::Window{"Inspector", &show_inspector, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove}) { draw_inspector(w); }
		if (!show_inspector) { m_inspecting = {}; }
	}

	if (auto popup = Popup{"scene_inspector.spawn_entity"}) {
		ImGui::Text("Spawn Entity");
		m_entity_name("Name");
		if (!m_entity_name.empty() && ImGui::Button("Spawn")) {
			scene.spawn({}, NodeCreateInfo{.name = std::string{m_entity_name.view()}});
			m_entity_name = {};
			popup.close_current();
		}
	}

	return m_inspecting;
}

void SceneGraph::camera_node() {
	auto flags = int{};
	flags |= (ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow);
	if (m_inspecting.type == Inspect::Type::eCamera) { flags |= ImGuiTreeNodeFlags_Selected; }
	if (imcpp::TreeNode::leaf("Camera", flags)) { m_inspecting.type = Inspect::Type::eCamera; }
}

bool SceneGraph::walk_node(Node& node) {
	auto node_locator = m_scene->node_locator();
	auto flags = int{};
	flags |= (ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow);
	if (node.entity && m_inspecting == node.entity) { flags |= ImGuiTreeNodeFlags_Selected; }
	if (node.children().empty()) { flags |= ImGuiTreeNodeFlags_Leaf; }
	auto tn = imcpp::TreeNode{node.name.c_str(), flags};
	if (node.entity) {
		if (ImGui::IsItemClicked()) { m_inspecting = {node.entity, Inspect::Type::eEntity}; }
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
			m_right_clicked = true;
			m_right_clicked_entity = node.entity;
		}
	}
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

void SceneGraph::draw_scene_tree(imcpp::OpenWindow) {
	auto node_locator = m_scene->node_locator();
	for (auto const& node : node_locator.roots()) {
		if (!walk_node(node_locator.get(node))) { return; }
	}

	if (m_right_clicked) {
		Popup::open("scene_inspector.right_click_entity");
		m_right_clicked = {};
	}

	if (auto popup = Popup{"scene_inspector.right_click_entity"}) {
		if (!m_scene->find(m_right_clicked_entity)) {
			m_right_clicked_entity = {};
			return popup.close_current();
		}
		if (ImGui::Button("Destroy")) {
			m_scene->destroy(m_right_clicked_entity);
			m_right_clicked_entity = {};
			popup.close_current();
		}
	}
}

void SceneGraph::draw_inspector(imcpp::OpenWindow w) {
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
		if (ImGui::Button("Attach...")) { Popup::open("scene_inspector.attach"); }
		break;
	}
	}

	if (auto popup = Popup{"scene_inspector.attach"}) {
		assert(m_inspecting.type == Inspect::Type::eEntity);
		auto* entity = m_scene->find(m_inspecting.entity);
		assert(entity);
		auto const& serializer = Service<Serializer>::locate();
		auto const& map = serializer.entries();
		for (auto const& [type_name, entry] : map) {
			if (ImGui::Selectable(type_name.c_str())) { m_scene->attach_to(*entity, type_name); }
		}
	}
}
} // namespace levk::imcpp
