#include <imgui.h>
#include <levk/asset/asset_loader.hpp>
#include <levk/engine.hpp>
#include <levk/entity.hpp>
#include <levk/imcpp/reflector.hpp>
#include <levk/impl/desktop_window.hpp>
#include <levk/impl/vulkan_device.hpp>
#include <levk/resources.hpp>
#include <levk/scene.hpp>
#include <levk/serializer.hpp>
#include <levk/service.hpp>
#include <levk/transform_controller.hpp>
#include <levk/util/error.hpp>
#include <levk/util/fixed_string.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/reader.hpp>
#include <levk/util/resource_map.hpp>
#include <levk/util/visitor.hpp>
#include <filesystem>

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

struct Inspect {
	enum class Type { eEntity, eCamera, eLights };

	Id<Entity> entity{};
	Type type{};

	explicit constexpr operator bool() const { return type != Type::eEntity || entity != Id<Entity>{}; }

	constexpr bool operator==(Id<Entity> id) const { return type == Type::eEntity && id == entity; }
	constexpr bool operator==(Type desired) const { return type == desired; }
};

void add_camera_node(Inspect& inspect) {
	auto flags = int{};
	flags |= (ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow);
	if (inspect.type == Inspect::Type::eCamera) { flags |= ImGuiTreeNodeFlags_Selected; }
	imcpp::TreeNode::leaf("Camera", flags);
	if (ImGui::IsItemClicked()) { inspect.type = Inspect::Type::eCamera; }
}

bool walk_node(Node::Locator& node_locator, Node& node, Inspect& inspect) {
	auto flags = int{};
	flags |= (ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow);
	if (node.entity && inspect == node.entity) { flags |= ImGuiTreeNodeFlags_Selected; }
	if (node.children().empty()) { flags |= ImGuiTreeNodeFlags_Leaf; }
	auto tn = imcpp::TreeNode{node.name.c_str(), flags};
	if (ImGui::IsItemClicked() && node.entity) { inspect = {node.entity, Inspect::Type::eEntity}; }
	auto const id = node.id().value();
	if (auto source = imcpp::DragDropSource{}) {
		ImGui::SetDragDropPayload("node", &id, sizeof(id));
		ImGui::Text("%s", node.name.c_str());
	}
	if (auto target = imcpp::DragDropTarget{}) {
		if (auto const* payload = ImGui::AcceptDragDropPayload("node")) {
			auto const child = *static_cast<std::size_t const*>(payload->Data);
			node_locator.reparent(node_locator.get(child), id);
			return false;
		}
	}
	if (tn) {
		for (auto& id : node.children()) {
			if (!walk_node(node_locator, node_locator.get(id), inspect)) { return false; }
		}
	}
	return true;
}

void draw_scene_tree(imcpp::OpenWindow, Node::Locator node_locator, Inspect& inspect) {
	for (auto const& node : node_locator.roots()) {
		if (!walk_node(node_locator, node_locator.get(node), inspect)) { return; }
	}
}

void draw_inspector(imcpp::OpenWindow w, Scene& scene, Inspect inspect) {
	switch (inspect.type) {
	case Inspect::Type::eCamera: {
		imcpp::TreeNode::leaf("Camera", ImGuiTreeNodeFlags_SpanFullWidth);
		if (auto tn = imcpp::TreeNode{"Transform", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen}) {
			Bool unified_scaling{true};
			imcpp::Reflector{w}(scene.camera.transform, unified_scaling, {});
		}
		break;
	}
	case Inspect::Type::eLights: {
		break;
	}
	default: {
		auto* entity = scene.find(inspect.entity);
		if (!entity) { return; }
		entity->inspect(w);
		break;
	}
	}
}

struct FreeCam {
	Ptr<Window> window{};
	glm::vec3 move_speed{10.0f};
	float look_speed{0.3f};

	glm::vec2 prev_cursor{};
	glm::vec2 pitch_yaw{};

	void tick(Transform& transform, Input const& input, Time dt) {
		auto data = transform.data();

		if (input.is_held(MouseButton::eRight)) {
			if (window->cursor_mode() != CursorMode::eDisabled) { window->set_cursor_mode(CursorMode::eDisabled); }
		} else {
			if (window->cursor_mode() == CursorMode::eDisabled) { window->set_cursor_mode(CursorMode::eNormal); }
		}

		auto dxy = glm::vec2{};
		if (window->cursor_mode() != CursorMode::eDisabled) {
			prev_cursor = input.cursor;
			pitch_yaw = glm::eulerAngles(transform.orientation());
		} else {
			dxy = input.cursor - prev_cursor;
			pitch_yaw -= look_speed * glm::vec2{glm::radians(dxy.y), glm::radians(dxy.x)};

			auto dxyz = glm::vec3{};
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
		}

		data.orientation = glm::vec3{pitch_yaw, 0.0f};
		transform.set_data(data);

		prev_cursor = input.cursor;
	}
};

struct Services {
	std::optional<Service<Engine>::Instance> engine{};
	std::optional<Service<Resources>::Instance> resources{};
};

std::string trim_to_uri(std::string_view full, std::string_view data) {
	auto const i = full.find(data);
	if (i == std::string_view::npos) { return {}; }
	full = full.substr(i + data.size());
	while (!full.empty() && full.front() == '/') { full = full.substr(1); }
	return std::string{full};
}

void run(fs::path data_path) {
	auto reader = FileReader{};
	reader.mount(data_path.generic_string());
	auto services = Services{};
	services.engine.emplace(make_engine(reader));
	services.resources.emplace(data_path.generic_string());
	auto& engine = Service<Engine>::locate();
	auto scene = std::make_unique<Scene>();
	auto free_cam = FreeCam{&engine.window()};
	auto inspect = Inspect{};

	engine.show();
	while (engine.is_running()) {
		auto frame = engine.next_frame();
		if (frame.state.focus_changed() && frame.state.is_in_focus()) { reader.reload_out_of_date(); }
		if (frame.state.input.chord(Key::eW, Key::eLeftControl) || frame.state.input.chord(Key::eW, Key::eRightControl)) { engine.shutdown(); }

		for (auto const& drop : frame.state.drops) {
			auto path = fs::path{drop};
			if (path.extension() == ".gltf") {
				auto export_path = path.filename().stem();
				scene->import_gltf(drop.c_str(), export_path.generic_string());
				break;
			}
			if (path.extension() == ".json") {
				auto uri = trim_to_uri(fs::path{drop}.generic_string(), data_path.generic_string());
				if (!uri.empty()) {
					auto const asset_type = AssetLoader::get_asset_type(drop.c_str());
					if (asset_type == "mesh") {
						scene->load_mesh_into_tree(uri);
					} else if (asset_type == "scene") {
						scene->from_json(drop.c_str());
					}
				}
				break;
			}
		}

		// tick
		scene->tick(frame.dt);
		free_cam.tick(scene->camera.transform, frame.state.input, frame.dt);
		if constexpr (debug_v) {
			static bool show_demo{true};
			if (show_demo) { ImGui::ShowDemoWindow(&show_demo); }
		}

		// test code
		if (frame.state.input.chord(Key::eS, Key::eLeftControl)) {
			auto json = dj::Json{};
			scene->serialize(json);
			auto filename = "test_scene.json";
			json.to_file((data_path / filename).string().c_str());
			logger::debug("Scene saved to {}", filename);
		}
		// test code

		ImGui::SetNextWindowSize({400.0f, 300.0f}, ImGuiCond_Once);
		{
			if (auto w = imcpp::Window{"Scene"}) {
				float scale = engine.device().info().render_scale;
				if (ImGui::DragFloat("Render Scale", &scale, 0.05f, render_scale_limit_v[0], render_scale_limit_v[1])) {
					engine.device().set_render_scale(scale);
				}
				ImGui::Separator();
				add_camera_node(inspect);
				draw_scene_tree(w, scene->node_locator(), inspect);
				if (imcpp::DragDropSource::is_active()) {
					imcpp::TreeNode::leaf("(Unparent)");
					if (auto target = imcpp::DragDropTarget{}) {
						if (auto const* payload = ImGui::AcceptDragDropPayload("node")) {
							auto const child = *static_cast<std::size_t const*>(payload->Data);
							auto node_locator = scene->node_locator();
							node_locator.reparent(node_locator.get(child), {});
						}
					}
				}
			}
		}
		if (bool show_inspector = static_cast<bool>(inspect)) {
			ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x - 500.0f, 0.0f});
			ImGui::SetNextWindowSize({500.0f, ImGui::GetIO().DisplaySize.y});
			if (auto w = imcpp::Window{"Inspector", &show_inspector}) { draw_inspector(w, *scene, inspect); }
			if (!show_inspector) { inspect = {}; }
		}

		engine.render(*scene, scene->camera, scene->lights, Rgba::from({0.1f, 0.1f, 0.1f, 1.0f}));
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
