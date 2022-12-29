#include <imgui.h>
#include <levk/engine.hpp>
#include <levk/entity.hpp>
#include <levk/imcpp/reflector.hpp>
#include <levk/impl/desktop_window.hpp>
#include <levk/impl/vulkan_device.hpp>
#include <levk/transform_controller.hpp>
#include <levk/util/error.hpp>
#include <levk/util/fixed_string.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/reader.hpp>
#include <levk/util/resource_map.hpp>
#include <levk/util/visitor.hpp>
#include <filesystem>

#include <experiment/scene.hpp>
#include <levk/import_asset.hpp>

namespace levk {
namespace fs = std::filesystem;

namespace {
Engine make_engine(Reader& reader) { return Engine{DesktopWindow{}, VulkanDevice{reader}}; }

fs::path find_data(fs::path exe) {
	auto check = [](fs::path const& prefix) {
		auto path = prefix / "data";
		if (fs::is_directory(path)) { return path; }
		path = prefix / "demo/data";
		if (fs::is_directory(path)) { return path; }
		return fs::path{};
	};
	while (!exe.empty()) {
		if (auto ret = check(exe); !ret.empty()) { return ret; }
		auto parent = exe.parent_path();
		if (exe == parent) { break; }
		exe = std::move(parent);
	}
	return {};
}

bool walk_node(experiment::Scene& scene, Node& node, Id<Node>& inspect) {
	auto flags = int{};
	flags |= (ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow);
	if (inspect == node.id()) { flags |= ImGuiTreeNodeFlags_Selected; }
	if (node.children().empty()) { flags |= ImGuiTreeNodeFlags_Leaf; }
	auto tn = imcpp::TreeNode{node.name.c_str(), flags};
	if (ImGui::IsItemClicked()) { inspect = node.id(); }
	auto const id = node.id().value();
	if (ImGui::BeginDragDropSource()) {
		ImGui::SetDragDropPayload("node", &id, sizeof(id));
		ImGui::EndDragDropSource();
	}
	if (ImGui::BeginDragDropTarget()) {
		if (auto const* payload = ImGui::AcceptDragDropPayload("entity")) {
			auto const child = *static_cast<std::size_t const*>(payload->Data);
			scene.tree.nodes.reparent(scene.tree.nodes.get(child), id);
			return false;
		}
		ImGui::EndDragDropTarget();
	}
	if (tn) {
		for (auto& id : node.children()) {
			if (!walk_node(scene, scene.tree.nodes.get(id), inspect)) { return false; }
		}
	}
	return true;
}

void draw_scene_tree(imcpp::NotClosed<imcpp::Window>, experiment::Scene& scene, Id<Node>& inspect) {
	for (auto const& e : scene.tree.nodes.roots()) {
		if (!walk_node(scene, scene.tree.nodes.get(e), inspect)) { return; }
	}
}

void draw_inspector(imcpp::NotClosed<imcpp::Window> w, experiment::Scene& scene, Id<Node> id) {
	if (!id) { return; }
	auto* node = scene.tree.nodes.find(id);
	if (!node) { return; }
	imcpp::TreeNode::leaf(node->name.c_str());
	if (auto tn = imcpp::TreeNode("Transform", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed)) {
		auto unified_scaling = Bool{true};
		imcpp::Reflector{w}(node->transform, unified_scaling, {true});
	}
	auto* entity = scene.tree.entities.find(node->entity);
	if (!entity) { return; }
	auto inspect_skin = [&](experiment::SkinnedMeshRenderer::Skin& skin) {
		auto const& skeleton = scene.mesh_resources->skeletons.get(skin.skeleton.source);
		imcpp::TreeNode::leaf(FixedString{"Skeleton: {}", skeleton.name}.c_str());
		if (skin.skeleton.animations.empty()) { return; }
		if (auto tn = imcpp::TreeNode("Animation")) {
			auto const preview = skin.enabled ? FixedString{"{}", skin.enabled->value()} : FixedString{"[None]"};
			if (ImGui::BeginCombo("Active", preview.c_str())) {
				if (ImGui::Selectable("[None]")) {
					skin.change_animation({});
				} else {
					for (std::size_t i = 0; i < skin.skeleton.animations.size(); ++i) {
						if (ImGui::Selectable(FixedString{"{}", i}.c_str(), skin.enabled && i == skin.enabled->value())) {
							skin.change_animation(i);
							break;
						}
					}
				}
				ImGui::EndCombo();
			}
			if (skin.enabled) {
				auto& animation = skin.skeleton.animations[*skin.enabled];
				ImGui::Text("%s", FixedString{"Duration: {:.1f}s", animation.duration()}.c_str());
				float const progress = animation.elapsed / animation.duration();
				ImGui::ProgressBar(progress);
			}
		}
	};
	if (auto* mesh_renderer = entity->find<experiment::MeshRenderer>()) {
		if (auto tn = imcpp::TreeNode("Mesh Renderer", ImGuiTreeNodeFlags_Framed)) {
			auto const visitor = Visitor{
				[&](experiment::StaticMeshRenderer& smr) {
					imcpp::TreeNode::leaf(FixedString{"Mesh: {}", scene.mesh_resources->static_meshes.get(smr.mesh).name}.c_str());
				},
				[&](experiment::SkinnedMeshRenderer& smr) {
					imcpp::TreeNode::leaf(FixedString{"Mesh: {}", scene.mesh_resources->skinned_meshes.get(smr.mesh).name}.c_str());
					inspect_skin(smr.skin);
				},
			};
			std::visit(visitor, mesh_renderer->renderer);
		}
	}
}

struct FreeCam {
	Ptr<Window> window{};
	glm::vec3 move_speed{10.0f};
	float look_speed{0.3f};

	glm::quat horz{glm::identity<glm::quat>()};
	glm::quat vert{glm::identity<glm::quat>()};
	glm::vec2 prev_cursor{};

	void tick(Transform& transform, Input const& input, Time dt) {
		auto data = transform.data();

		if (input.is_held(MouseButton::eRight)) {
			if (window->cursor_mode() != CursorMode::eDisabled) { window->set_cursor_mode(CursorMode::eDisabled); }
		} else {
			if (window->cursor_mode() == CursorMode::eDisabled) { window->set_cursor_mode(CursorMode::eNormal); }
		}

		if (window->cursor_mode() != CursorMode::eDisabled) {
			prev_cursor = input.cursor;
			return;
		}

		auto const dxy = input.cursor - prev_cursor;
		if (std::abs(dxy.x) > 0.0f) { horz = glm::rotate(horz, glm::radians(-dxy.x) * look_speed, up_v); }
		if (std::abs(dxy.y) > 0.0f) { vert = glm::rotate(vert, glm::radians(-dxy.y) * look_speed, right_v); }

		auto dxyz = glm::vec3{};
		data.orientation = horz * vert;
		auto front = data.orientation * front_v;
		auto right = data.orientation * right_v;
		auto up = data.orientation * up_v;

		if (input.is_held(Key::eW) || input.is_held(Key::eUp)) { dxyz.z -= 1.0f; }
		if (input.is_held(Key::eS) || input.is_held(Key::eDown)) { dxyz.z += 1.0f; }
		if (input.is_held(Key::eA) || input.is_held(Key::eLeft)) { dxyz.x -= 1.0f; }
		if (input.is_held(Key::eD) || input.is_held(Key::eRight)) { dxyz.x += 1.0f; }
		if (input.is_held(Key::eQ)) { dxyz.y -= 1.0f; }
		if (input.is_held(Key::eE)) { dxyz.y += 1.0f; }
		if (std::abs(dxyz.x) > 0.0f || std::abs(dxyz.y) > 0.0f || std::abs(dxyz.z) > 0.0f) {
			dxyz = glm::normalize(dxyz);
			auto const factor = dt.count() * move_speed;
			data.position += factor * front * dxyz.z;
			data.position += factor * right * dxyz.x;
			data.position += factor * up * dxyz.y;
		}

		data.orientation = data.orientation;

		transform.set_data(data);

		prev_cursor = input.cursor;
	}
};

void run(fs::path data_path) {
	auto reader = FileReader{};
	reader.mount(data_path.generic_string());
	auto engine = make_engine(reader);
	auto mesh_resources = std::make_unique<MeshResources>();
	auto scene = std::make_unique<experiment::Scene>(engine, *mesh_resources);
	auto free_cam = FreeCam{&engine.window()};
	auto inspect = Id<Node>{};

	engine.show();
	while (engine.is_running()) {
		auto frame = engine.next_frame();
		if (frame.state.focus_changed() && frame.state.is_in_focus()) { reader.reload_out_of_date(); }
		if (frame.state.input.chord(Key::eW, Key::eLeftControl) || frame.state.input.chord(Key::eW, Key::eRightControl)) { engine.shutdown(); }

		for (auto const& drop : frame.state.drops) {
			auto path = fs::path{drop};
			if (path.extension() == ".gltf") {
				auto export_path = data_path / path.filename().stem();
				scene->import_gltf(drop.c_str(), export_path.string().c_str());
				break;
			}
			if (path.extension() == ".json") {
				auto json = dj::Json::from_file(drop.c_str());
				if (json["asset_type"].as_string() == "mesh") {
					auto loader = AssetLoader{engine.device(), *mesh_resources};
					auto const res = loader.try_load_mesh(drop.c_str());
					auto visitor = Visitor{
						[&](auto id) { scene->add_mesh_to_tree(id); },
						[](std::monostate) {},
					};
					std::visit(visitor, res);
				}
				break;
			}
		}

		// tick
		scene->tick(frame.dt);
		free_cam.tick(engine.camera.transform, frame.state.input, frame.dt);
		if constexpr (debug_v) {
			static bool show_demo{true};
			if (show_demo) { ImGui::ShowDemoWindow(&show_demo); }
		}

		ImGui::SetNextWindowSize({400.0f, 300.0f}, ImGuiCond_Once);
		if (auto w = imcpp::Window{"Scene"}) {
			float scale = engine.device().info().render_scale;
			if (ImGui::DragFloat("Render Scale", &scale, 0.05f, render_scale_limit_v[0], render_scale_limit_v[1])) { engine.device().set_render_scale(scale); }
			ImGui::Separator();
			draw_scene_tree(w, *scene, inspect);
		}
		if (bool show_inspector = inspect) {
			ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x - 500.0f, 0.0f});
			ImGui::SetNextWindowSize({500.0f, ImGui::GetIO().DisplaySize.y});
			if (auto w = imcpp::Window{"Inspector", &show_inspector}) { draw_inspector(w, *scene, inspect); }
			if (!show_inspector) { inspect = {}; }
		}

		auto renderer = experiment::Scene::Renderer{*scene};
		engine.render(renderer, Rgba::from({0.1f, 0.1f, 0.1f, 1.0f}));
	}
}
} // namespace
} // namespace levk

int main([[maybe_unused]] int argc, char** argv) {
	namespace logger = levk::logger;
	auto instance = logger::Instance{};
	try {
		assert(argc > 0);
		levk::run(levk::find_data(argv[0]));
	} catch (levk::Error const& e) {
		logger::error("Runtime error: {}", e.what());
		return EXIT_FAILURE;
	} catch (std::exception const& e) {
		logger::error("Fatal error: {}", e.what());
		return EXIT_FAILURE;
	} catch (...) {
		logger::error("Unknown error");
		return EXIT_FAILURE;
	}
}
