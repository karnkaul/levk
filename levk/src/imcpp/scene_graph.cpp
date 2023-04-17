#include <imgui.h>
#include <levk/imcpp/drag_drop.hpp>
#include <levk/imcpp/reflector.hpp>
#include <levk/imcpp/scene_graph.hpp>
#include <levk/scene/scene.hpp>
#include <levk/scene/scene_manager.hpp>
#include <levk/service.hpp>
#include <levk/util/fixed_string.hpp>

namespace levk::imcpp {
bool SceneGraph::check_stale() {
	bool ret = false;
	if (m_scene != m_prev) {
		m_prev = m_scene;
		m_inspector.target = {};
		ret = true;
	}
	if (m_inspector.target.type == Inspector::Type::eEntity && !m_scene->find_entity(m_inspector.target.entity)) { m_inspector.target = {}; }
	return ret;
}

Inspector::Target SceneGraph::draw_to(NotClosed<Window> w, Scene& scene) {
	m_scene = &scene;
	check_stale();

	if (ImGui::SliderFloat("Inspector Width", &m_inspector.width_pct, 0.1f, 0.5f, "%.3f")) {
		m_inspector.width_pct = std::clamp(m_inspector.width_pct, 0.1f, 0.5f);
	}

	ImGui::Separator();
	if (ImGui::Button("Spawn")) { Popup::open("scene_graph.spawn_entity"); }

	ImGui::Separator();
	standalone_node("SceneCamera", Inspector::Type::eSceneCamera);
	standalone_node("Lights", Inspector::Type::eLights);
	draw_scene_tree(w);
	handle_popups();
	if (auto* payload = ImGui::GetDragDropPayload()) {
		auto* scene_manager = Service<SceneManager>::find();
		if (payload->IsDataType("node")) {
			imcpp::TreeNode::leaf("(Unparent)");
			if (auto target = imcpp::DragDrop::Target{}) {
				if (auto const* node_id = imcpp::DragDrop::accept<std::size_t>("node")) {
					auto node_locator = scene.node_locator();
					node_locator.reparent(node_locator.get(*node_id), {});
				}
			}
		} else if (payload->IsDataType("skinned_mesh")) {
			if (scene_manager) {
				imcpp::TreeNode::leaf("(Instantiate skeleton)");
				if (auto target = imcpp::DragDrop::Target{}) {
					if (auto uri = imcpp::DragDrop::accept_string("skinned_mesh"); !uri.empty()) { scene_manager->load_into_tree(Uri<SkinnedMesh>{uri}); }
				}
			}
		}
	}

	m_inspector.display(scene);

	return m_inspector.target;
}

void SceneGraph::standalone_node(char const* label, Inspector::Type type) {
	auto flags = int{};
	flags |= ImGuiTreeNodeFlags_SpanFullWidth;
	if (m_inspector.target.type == type) { flags |= ImGuiTreeNodeFlags_Selected; }
	if (imcpp::TreeNode::leaf(label, flags)) { m_inspector.target.type = type; }
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
		m_right_clicked_target = {.type = type};
		Popup::open("scene_graph.right_click");
	}
}

bool SceneGraph::walk_node(Node& node) {
	auto node_locator = m_scene->node_locator();
	auto flags = int{};
	flags |= (ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow);
	if (node.entity_id && m_inspector.target == node.entity_id) { flags |= ImGuiTreeNodeFlags_Selected; }
	if (node.children().empty()) { flags |= ImGuiTreeNodeFlags_Leaf; }
	auto tn = imcpp::TreeNode{node.name.c_str(), flags};
	if (node.entity_id) {
		auto target = Inspector::Target{node.entity_id, Inspector::Type::eEntity};
		if (ImGui::IsItemClicked()) { m_inspector.target = target; }
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
			m_right_clicked = true;
			m_right_clicked_target = target;
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
		Popup::open("scene_graph.right_click");
		m_right_clicked = {};
	}
}

void SceneGraph::handle_popups() {
	if (auto popup = Popup{"scene_graph.spawn_entity"}) {
		ImGui::Text("Spawn Entity");
		m_entity_name("Name");
		if (!m_entity_name.empty() && ImGui::Button("Spawn")) {
			m_scene->spawn(NodeCreateInfo{.name = std::string{m_entity_name.view()}});
			m_entity_name = {};
			popup.close_current();
		}
	}

	if (auto popup = Popup{"scene_graph.right_click"}) {
		if (m_right_clicked_target.type == Inspector::Type::eEntity && !m_scene->find_entity(m_right_clicked_target.entity)) {
			m_right_clicked_target = {};
			return popup.close_current();
		}
		if (ImGui::Selectable("Inspect")) {
			m_inspector.target = m_right_clicked_target;
			m_right_clicked_target = {};
			popup.close_current();
		}
		if (m_right_clicked_target.type == Inspector::Type::eEntity && ImGui::Selectable("Destroy")) {
			m_scene->destroy_entity(m_right_clicked_target.entity);
			m_right_clicked_target = {};
			popup.close_current();
		}
	}
}
} // namespace levk::imcpp
