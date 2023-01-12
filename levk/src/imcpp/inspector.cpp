#include <imgui.h>
#include <levk/imcpp/inspector.hpp>
#include <levk/imcpp/reflector.hpp>
#include <levk/scene.hpp>
#include <levk/service.hpp>

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
		entity->inspect(w);
		if (ImGui::Button("Attach...")) { Popup::open("inspector.attach"); }
		break;
	}
	}

	if (auto popup = Popup{"inspector.attach"}) {
		assert(target.type == Type::eEntity);
		auto* entity = scene.find(target.entity);
		assert(entity);
		ImGui::Text("TODO...");
		// auto const& serializer = Service<Serializer>::locate();
		// auto const& map = serializer.entries();
		// for (auto const& [type_name, entry] : map) {
		// 	if (ImGui::Selectable(type_name.c_str())) { scene.attach_to(*entity, type_name); }
		// }
	}
}
} // namespace levk::imcpp
