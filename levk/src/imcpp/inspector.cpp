#include <imgui.h>
#include <levk/component_factory.hpp>
#include <levk/imcpp/inspector.hpp>
#include <levk/imcpp/reflector.hpp>
#include <levk/scene.hpp>
#include <levk/service.hpp>
#include <levk/util/fixed_string.hpp>

namespace levk::imcpp {
void Inspector::display(Scene& scene) {
	if (bool show_inspector = static_cast<bool>(target)) {
		auto const width = ImGui::GetMainViewport()->Size.x * width_pct;
		ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x - width, 0.0f});
		ImGui::SetNextWindowSize({width, ImGui::GetIO().DisplaySize.y});
		if (auto w = Window{"Inspector", &show_inspector, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove}) { draw_to(w, scene); }
		if (!show_inspector) { target = {}; }
	}
}

void Inspector::draw_to(NotClosed<Window> w, Scene& scene) {
	switch (target.type) {
	case Type::eCamera: {
		imcpp::TreeNode::leaf("Camera", ImGuiTreeNodeFlags_SpanFullWidth);
		if (auto tn = imcpp::TreeNode{"Transform", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen}) {
			Bool unified_scaling{true};
			imcpp::Reflector{w}(scene.camera.transform, unified_scaling, {});
		}
		break;
	}
	case Type::eLights: {
		break;
	}
	default: {
		auto* entity = scene.find(target.entity);
		if (!entity) { return; }
		auto* node = scene.node_locator().find(entity->node_id());
		if (!node) { return; }
		imcpp::TreeNode::leaf(node->name.c_str());
		ImGui::Checkbox("Active", &entity->active);
		if (auto tn = imcpp::TreeNode("Transform", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed)) {
			auto unified_scaling = Bool{true};
			imcpp::Reflector{w}(node->transform, unified_scaling, {true});
		}
		auto inspect_component = [&](TypeId::value_type type_id, Component& component) {
			if (auto tn = imcpp::TreeNode{FixedString<128>{component.type_name()}.c_str(), ImGuiTreeNodeFlags_Framed}) {
				component.inspect(w);
				if (type_id) {
					ImGui::Separator();
					if (ImGui::Button("Detach")) {
						scene.detach_from(*entity, type_id);
						return false;
					}
				}
			}
			return true;
		};
		for (auto const& [type_id, component] : entity->component_map()) {
			if (!inspect_component(type_id, *component)) { break; }
		}
		if (ImGui::Button("Attach...")) { Popup::open("inspector.attach"); }
		break;
	}
	}

	if (auto popup = Popup{"inspector.attach"}) {
		assert(target.type == Type::eEntity);
		auto* entity = scene.find(target.entity);
		assert(entity);
		ImGui::Text("TODO...");
		auto const& component_factory = Service<ComponentFactory>::locate();
		auto const& map = component_factory.entries();
		for (auto const& [type_name, entry] : map) {
			if (ImGui::Selectable(type_name.c_str())) { entity->attach(entry.factory()); }
		}
	}
}
} // namespace levk::imcpp
