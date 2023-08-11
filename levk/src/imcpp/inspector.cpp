#include <imgui.h>
#include <levk/asset/asset_providers.hpp>
#include <levk/imcpp/drag_drop.hpp>
#include <levk/imcpp/inspector.hpp>
#include <levk/imcpp/reflector.hpp>
#include <levk/io/serializer.hpp>
#include <levk/scene/freecam_controller.hpp>
#include <levk/scene/scene.hpp>
#include <levk/scene/shape_renderer.hpp>
#include <levk/scene/skeleton_controller.hpp>
#include <levk/scene/skinned_mesh_renderer.hpp>
#include <levk/scene/static_mesh_renderer.hpp>
#include <levk/service.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/fixed_string.hpp>
#include <levk/util/visitor.hpp>

namespace levk::imcpp {
namespace {
void inspect(OpenWindow w, std::vector<Transform>& out_instances) {
	if (auto tn = imcpp::TreeNode{"Instances"}) {
		auto const reflector = imcpp::Reflector{w};
		Bool unified_scaling{true};
		auto to_remove = std::optional<std::size_t>{};
		for (auto [instance, index] : enumerate(out_instances)) {
			if (auto tn = imcpp::TreeNode{FixedString{"[{}]", index}.c_str()}) {
				reflector(instance, unified_scaling, {false});
				if (imcpp::small_button_red("X")) { to_remove = index; }
			}
		}
		if (to_remove) { out_instances.erase(out_instances.begin() + static_cast<std::ptrdiff_t>(*to_remove)); }
		if (ImGui::Button("+")) { out_instances.push_back({}); }
	}
}

void inspect(OpenWindow w, ShapeRenderer& shape_renderer) {
	auto& shape = const_cast<Shape&>(shape_renderer.shape());
	if (auto tn = TreeNode{FixedString{"{}###Shape", shape.type_name()}.c_str()}) {
		shape.inspect(w);
		if (ImGui::SmallButton("Refresh")) { shape_renderer.refresh_geometry(); }
	}
	inspect(w, shape_renderer.instances);
	if (ImGui::Button("Change Shape")) { imcpp::Popup::open("shape_renderer.set_shape"); }

	auto* serializer = Service<Serializer>::find();
	if (serializer) {
		if (auto popup = Popup{"shape_renderer.set_shape"}) {
			for (auto const type_name : serializer->shape_types()) {
				if (ImGui::Selectable(type_name.data())) {
					auto shape = serializer->try_make<Shape>(std::string{type_name});
					if (shape) { shape_renderer.set_shape(std::move(shape)); }
				}
			}
		}
	}
}

void inspect(OpenWindow w, StaticMeshRenderer& mesh_renderer) {
	auto* asset_providers = Service<AssetProviders>::find();
	auto label = FixedString{};
	if (asset_providers) {
		if (auto const* static_mesh = asset_providers->static_mesh().find(mesh_renderer.mesh_uri)) { label = static_mesh->name; }
	}
	TreeNode::leaf(FixedString{"Mesh: {}", label}.c_str());
	if (mesh_renderer.mesh_uri && ImGui::IsItemClicked(ImGuiMouseButton_Right)) { Popup::open("static_mesh_renderer.right_click"); }
	if (auto drop = DragDrop::Target{}) {
		if (auto const in_mesh = DragDrop::accept_string("static_mesh"); !in_mesh.empty()) { mesh_renderer.mesh_uri = in_mesh; }
	}

	inspect(w, mesh_renderer.instances);

	if (auto popup = imcpp::Popup{"static_mesh_renderer.right_click"}) {
		if (ImGui::Selectable("Unset")) {
			mesh_renderer.mesh_uri = {};
			popup.close_current();
		}
	}
}

void inspect(OpenWindow w, SkinnedMeshRenderer& mesh_renderer) {
	auto* asset_providers = Service<AssetProviders>::find();
	auto label = FixedString{};
	if (asset_providers) {
		if (auto const* mesh = asset_providers->skinned_mesh().find(mesh_renderer.mesh_uri())) { label = mesh->name; }
	}
	TreeNode::leaf(FixedString{"Mesh: {}", label}.c_str());
	if (mesh_renderer.mesh_uri() && ImGui::IsItemClicked(ImGuiMouseButton_Right)) { Popup::open("skinned_mesh_renderer.right_click"); }
	if (auto drop = DragDrop::Target{}) {
		if (auto const in_mesh = DragDrop::accept_string("skinned_mesh"); !in_mesh.empty()) { mesh_renderer.set_mesh_uri(in_mesh); }
	}

	inspect(w, mesh_renderer.instances);

	if (auto popup = imcpp::Popup{"skinned_mesh_renderer.right_click"}) {
		if (ImGui::Selectable("Unset")) {
			mesh_renderer.set_mesh_uri({});
			popup.close_current();
		}
	}
}

void inspect(OpenWindow, Entity const& entity, SkeletonController& skeleton_controller) {
	auto const* skinned_mesh_renderer = entity.find_component<SkinnedMeshRenderer>();
	if (!skinned_mesh_renderer) { return; }
	auto const preview = skeleton_controller.enabled ? FixedString{"{}", skeleton_controller.enabled->value()} : FixedString{"[None]"};
	if (auto combo = imcpp::Combo{"Active", preview.c_str()}) {
		if (ImGui::Selectable("[None]")) {
			skeleton_controller.change_animation({});
		} else {
			for (std::size_t i = 0; i < skinned_mesh_renderer->skeleton().animations.size(); ++i) {
				if (ImGui::Selectable(FixedString{"{}", i}.c_str(), skeleton_controller.enabled && i == skeleton_controller.enabled->value())) {
					skeleton_controller.change_animation(i);
					break;
				}
			}
		}
	}
	auto const* asset_providers = Service<AssetProviders>::find();
	auto& animations = skinned_mesh_renderer->skeleton().animations;
	if (asset_providers && skeleton_controller.enabled && *skeleton_controller.enabled < animations.size()) {
		auto* animation = asset_providers->skeletal_animation().find(animations[*skeleton_controller.enabled]);
		ImGui::Text("%s", FixedString{"Duration: {:.1f}s", animation->animation.duration().count()}.c_str());
		float const progress = skeleton_controller.elapsed / animation->animation.duration();
		ImGui::ProgressBar(progress);
	}
}

void inspect(OpenWindow w, FreecamController& freecam_controller) {
	Reflector{w}("Move Speed", freecam_controller.move_speed);
	ImGui::DragFloat("Look Speed", &freecam_controller.look_speed);
	auto degrees = freecam_controller.pitch.to_degrees();
	if (ImGui::DragFloat("Pitch", &degrees.value, 0.25f, -89.0f, 89.0f)) { freecam_controller.pitch = degrees; }
	degrees = freecam_controller.yaw.to_degrees();
	if (ImGui::DragFloat("Yaw", &degrees.value, 0.25f, -180.0f, 180.0f)) { freecam_controller.yaw = degrees; }
}

void inspect(OpenWindow w, ColliderAabb& collider) { Reflector{w}("Size", collider.aabb_size, 0.25f); }

template <typename Type>
bool detach(Entity& entity) {
	ImGui::Separator();
	if (ImGui::Button("Detach")) {
		entity.detach<Type>();
		return false;
	}
	return true;
}

bool inspect_component(OpenWindow w, Entity& entity, Component& component) {
	auto shape_renderer = Ptr<ShapeRenderer>{};
	if (shape_renderer = dynamic_cast<ShapeRenderer*>(&component); shape_renderer) {
		if (auto tn = imcpp::TreeNode{"ShapeRenderer", ImGuiTreeNodeFlags_Framed}) {
			inspect(w, *shape_renderer);
			return detach<ShapeRenderer>(entity);
		}
	} else if (auto* mesh_renderer = dynamic_cast<StaticMeshRenderer*>(&component)) {
		if (auto tn = imcpp::TreeNode{"StaticMeshRenderer", ImGuiTreeNodeFlags_Framed}) {
			inspect(w, *mesh_renderer);
			return detach<StaticMeshRenderer>(entity);
		}
	} else if (auto* mesh_renderer = dynamic_cast<SkinnedMeshRenderer*>(&component)) {
		if (auto tn = imcpp::TreeNode{"SkinnedMeshRenderer", ImGuiTreeNodeFlags_Framed}) {
			inspect(w, *mesh_renderer);
			return detach<SkinnedMeshRenderer>(entity);
		}
	} else if (auto* skeleton_controller = dynamic_cast<SkeletonController*>(&component)) {
		if (auto tn = imcpp::TreeNode{"SkeletonController", ImGuiTreeNodeFlags_Framed}) {
			inspect(w, entity, *skeleton_controller);
			return detach<SkeletonController>(entity);
		}
	} else if (auto* freecam_controller = dynamic_cast<FreecamController*>(&component)) {
		if (auto tn = imcpp::TreeNode{"FreecamController", ImGuiTreeNodeFlags_Framed}) {
			inspect(w, *freecam_controller);
			return detach<FreecamController>(entity);
		}
	} else if (auto* collider = dynamic_cast<ColliderAabb*>(&component)) {
		if (auto tn = imcpp::TreeNode{"ColliderAabb", ImGuiTreeNodeFlags_Framed}) {
			inspect(w, *collider);
			return detach<ColliderAabb>(entity);
		}
	}

	return true;
}

template <typename T>
bool attach_selectable(char const* label, Entity& out) {
	if (!out.contains<T>() && ImGui::Selectable(label)) {
		out.attach(std::make_unique<T>());
		return true;
	}
	return false;
}
} // namespace

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
	case Type::eSceneCamera: {
		imcpp::TreeNode::leaf("Scene Camera", ImGuiTreeNodeFlags_SpanFullWidth);
		if (auto tn = imcpp::TreeNode{"Transform", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen}) {
			Bool unified_scaling{true};
			auto target = static_cast<int>(scene.camera.target);
			if (ImGui::InputInt("Target", &target)) { scene.camera.target = static_cast<Id<Entity>::id_type>(target); }
			imcpp::Reflector{w}(scene.camera.transform, unified_scaling, {});
		}
		ImGui::DragFloat("Exposure", &scene.camera.exposure, 0.25f, 1.0f, 100.0f);
		std::string_view const type = std::holds_alternative<Camera::Orthographic>(scene.camera.type) ? "Orthographic" : "Perspective";
		if (auto combo = imcpp::Combo{"Type", type.data()}) {
			if (combo.item("Perspective", {type == "Perspective"})) { scene.camera.type = Camera::Perspective{}; }
			if (combo.item("Orthographic", {type == "Orthographic"})) { scene.camera.type = Camera::Orthographic{}; }
		}
		std::visit([w](auto& camera) { Reflector{w}(camera); }, scene.camera.type);
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
		auto* entity = scene.find_entity(target.entity);
		if (!entity) { return; }
		auto* node = scene.node_locator().find(entity->node_id());
		if (!node) { return; }
		ImGui::Text("%s", FixedString{"{}", entity->id()}.c_str());
		auto& entity_name = get_entity_name(target.entity, node->name);
		if (entity_name("Name")) { node->name = entity_name.view(); }
		ImGui::Checkbox("Active", &entity->is_active);
		if (auto tn = imcpp::TreeNode("Transform", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed)) {
			auto unified_scaling = Bool{true};
			imcpp::Reflector{w}(node->transform, unified_scaling, {true});
		}
		for (auto const& [type_id, component] : entity->component_map()) {
			if (!inspect_component(w, *entity, *component)) { break; }
		}
		if (ImGui::Button("Attach...")) { Popup::open("inspector.attach"); }
		break;
	}
	}

	if (auto popup = Popup{"inspector.attach"}) {
		assert(target.type == Type::eEntity);
		auto* entity = scene.find_entity(target.entity);
		assert(entity);
		bool close = attach_selectable<ShapeRenderer>("ShapeRenderer", *entity);
		close |= attach_selectable<StaticMeshRenderer>("StaticMeshRenderer", *entity);
		close |= attach_selectable<SkinnedMeshRenderer>("SkinnedMeshRenderer", *entity);
		close |= attach_selectable<SkeletonController>("SkeletonController", *entity);
		close |= attach_selectable<FreecamController>("FreecamController", *entity);
		close |= attach_selectable<ColliderAabb>("ColliderAabb", *entity);
		if (close) { popup.close_current(); }
	}
}

InputText<>& Inspector::get_entity_name(Id<Entity> id, std::string_view name) {
	if (m_entity_name.previous != id) {
		m_entity_name.previous = id;
		m_entity_name.input_text.set(name);
	}
	return m_entity_name.input_text;
}
} // namespace levk::imcpp
