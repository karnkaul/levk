#include <imgui.h>
#include <levk/imcpp/inspector.hpp>
#include <levk/imcpp/reflector.hpp>
#include <levk/io/serializer.hpp>
#include <levk/scene/scene.hpp>
#include <levk/service.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/fixed_string.hpp>
#include <levk/util/visitor.hpp>

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
		ImGui::DragFloat("Exposure", &scene.camera.exposure, 0.25f, 1.0f, 100.0f);
		std::string_view const type = std::holds_alternative<Camera::Orthographic>(scene.camera.type) ? "Orthographic" : "Perspective";
		if (auto combo = imcpp::Combo{"Type", type.data()}) {
			if (combo.item("Perspective", {type == "Perspective"})) { scene.camera.type = Camera::Perspective{}; }
			if (combo.item("Orthographic", {type == "Orthographic"})) { scene.camera.type = Camera::Orthographic{}; }
		}
		auto const visitor = Visitor{
			[](Camera::Perspective& perspective) {
				auto degrees = perspective.field_of_view.to_degrees();
				if (ImGui::DragFloat("Field of View", &degrees.value, 0.25f, 10.0f, 75.0f)) { perspective.field_of_view = degrees; }
				ImGui::DragFloat("Near plane", &perspective.view_plane.near, 0.1f, 0.1f, 10.0f);
				ImGui::DragFloat("Far plane", &perspective.view_plane.far, 1.0f, 20.0f, 1000.0f);
			},
			[](Camera::Orthographic& orthographic) {
				ImGui::DragFloat("Near plane", &orthographic.view_plane.near, 0.1f, 0.1f, 10.0f);
				ImGui::DragFloat("Far plane", &orthographic.view_plane.far, 1.0f, 20.0f, 1000.0f);
			},
		};
		std::visit(visitor, scene.camera.type);
		break;
	}
	case Type::eLights: {
		imcpp::TreeNode::leaf("Lights", ImGuiTreeNodeFlags_SpanFullWidth);
		auto const inspect_dir_light = [w](DirLight& dir_light) {
			imcpp::Reflector{w}(dir_light.rgb, {false});
			imcpp::Reflector{w}("Direction", dir_light.direction);
		};
		if (auto tn = imcpp::TreeNode{"Primary", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen}) { inspect_dir_light(scene.lights.primary); }
		if (auto tn = imcpp::TreeNode{"Directional", ImGuiTreeNodeFlags_Framed}) {
			auto to_remove = std::optional<std::size_t>{};
			for (auto [dir_light, index] : enumerate(scene.lights.dir_lights)) {
				if (auto tn = TreeNode{FixedString{"[{}]", index}.c_str()}) {
					inspect_dir_light(dir_light);
					if (small_button_red("X")) { to_remove = index; }
				}
			}
			if (to_remove) { scene.lights.dir_lights.erase(scene.lights.dir_lights.begin() + static_cast<std::ptrdiff_t>(*to_remove)); }
			ImGui::Separator();
			if (ImGui::Button("Add")) { scene.lights.dir_lights.push_back({}); }
		}
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
		auto const& serializer = Service<Serializer>::locate();
		auto const type_names = serializer.type_names_by_tag(Serializer::Tag::eComponent);
		for (auto const& type_name : type_names) {
			if (ImGui::Selectable(type_name.data())) { serializer.attach(*entity, std::string{type_name}); }
		}
	}
}
} // namespace levk::imcpp
