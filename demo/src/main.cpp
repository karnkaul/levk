#include <imgui.h>
#include <levk/asset/asset_loader.hpp>
#include <levk/engine.hpp>
#include <levk/entity.hpp>
#include <levk/imcpp/reflector.hpp>
#include <levk/resources.hpp>
#include <levk/scene.hpp>
#include <levk/serializer.hpp>
#include <levk/service.hpp>
#include <levk/transform_controller.hpp>
#include <levk/util/error.hpp>
#include <levk/util/fixed_string.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/reader.hpp>
#include <levk/util/visitor.hpp>
#include <filesystem>

#include <levk/imcpp/engine_status.hpp>
#include <levk/imcpp/log_renderer.hpp>
#include <levk/imcpp/resource_list.hpp>
#include <levk/imcpp/scene_graph.hpp>
#include <levk/runtime.hpp>
#include <main_menu.hpp>

namespace levk {
namespace fs = std::filesystem;

namespace {
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

class Editor {
  public:
	Editor(std::string data_path);

	void save_scene() const;

	void tick(Frame const& frame);
	void render(Rgba clear = black_v);

	FileReader reader{};
	std::string data_path{};
	Runtime runtime;

	Scene scene{};
	FreeCam free_cam{};
	imcpp::ResourceList resource_list{};
	imcpp::SceneGraph scene_graph{};
	imcpp::EngineStatus engine_status{};
	imcpp::LogRenderer log_renderer{};
	MainMenu main_menu{};
};

Editor::Editor(std::string data_path)
	: reader(data_path), data_path(std::move(data_path)), runtime(make_desktop_runtime(reader)), free_cam{&runtime.engine.get().window()} {
	auto file_menu = [&] {
		if (ImGui::MenuItem("Save", nullptr, false, !scene.empty())) { save_scene(); }
		ImGui::Separator();
		if (ImGui::MenuItem("Exit")) { runtime.shutdown(); }
	};

	main_menu.menus.lists.push_back(MainMenu::List{.label = "File", .callback = file_menu});

	main_menu.menus.window = {
		imcpp::EditorWindow{.label = "Scene", .draw_to = [&](imcpp::OpenWindow w) { scene_graph.draw_to(w, scene); }},
		imcpp::EditorWindow{.label = "Resources", .draw_to = [&](imcpp::OpenWindow w) { resource_list.draw_to(w, Service<Resources>::locate()); }},
		imcpp::EditorWindow{
			.label = "Engine", .init_size = {350.0f, 250.0f}, .draw_to = [&](imcpp::OpenWindow w) { engine_status.draw_to(w, runtime.engine.get()); }},
		imcpp::EditorWindow{.label = "Log", .init_size = {600.0f, 300.0f}, .draw_to = [&](imcpp::OpenWindow w) { log_renderer.draw_to(w); }},
	};
	if constexpr (debug_v) {
		main_menu.menus.window.push_back(MainMenu::Separator{});
		main_menu.menus.window.push_back(MainMenu::Custom{.label = "Dear ImGui Demo", .draw = [](bool& show) { ImGui::ShowDemoWindow(&show); }});
	}
}

void Editor::save_scene() const {
	auto json = dj::Json{};
	scene.serialize(json);
	auto filename = "test_scene.json";
	json.to_file((fs::path{data_path} / filename).string().c_str());
	logger::debug("Scene saved to {}", filename);
}

void Editor::tick(Frame const& frame) {
	scene.tick(frame.dt);
	free_cam.tick(scene.camera.transform, frame.state.input, frame.dt);

	// test code
	if (frame.state.input.chord(Key::eS, Key::eLeftControl)) { save_scene(); }
	// test code

	auto result = main_menu.display_menu();
	switch (result.action) {
	case MainMenu::Action::eExit: runtime.shutdown(); break;
	default: break;
	}

	main_menu.display_windows();
}

void Editor::render(Rgba clear) { runtime.render(scene, clear); }

std::string trim_to_uri(std::string_view full, std::string_view data) {
	auto const i = full.find(data);
	if (i == std::string_view::npos) { return {}; }
	full = full.substr(i + data.size());
	while (!full.empty() && full.front() == '/') { full = full.substr(1); }
	return std::string{full};
}

void run(std::string data_path) {
	auto editor = Editor{std::move(data_path)};

	editor.runtime.show();
	while (editor.runtime.is_running()) {
		auto frame = editor.runtime.next_frame();
		if (frame.state.focus_changed() && frame.state.is_in_focus()) { editor.reader.reload_out_of_date(); }
		if (frame.state.input.chord(Key::eW, Key::eLeftControl) || frame.state.input.chord(Key::eW, Key::eRightControl)) { editor.runtime.shutdown(); }

		for (auto const& drop : frame.state.drops) {
			auto path = fs::path{drop};
			if (path.extension() == ".gltf") {
				auto export_path = path.filename().stem();
				editor.scene.import_gltf(drop.c_str(), editor.data_path, export_path.generic_string());
				break;
			}
			if (path.extension() == ".json") {
				auto uri = trim_to_uri(fs::path{drop}.generic_string(), editor.data_path);
				if (!uri.empty()) {
					auto const asset_type = AssetLoader::get_asset_type(editor.reader, drop);
					if (asset_type == "mesh") {
						editor.scene.load_mesh_into_tree(uri);
					} else if (asset_type == "scene") {
						editor.scene.from_json(drop.c_str());
					}
				}
				break;
			}
		}

		// tick
		editor.tick(frame);

		// render
		editor.render(Rgba::from({0.1f, 0.1f, 0.1f, 1.0f}));
	}
}
} // namespace
} // namespace levk

int main([[maybe_unused]] int argc, char** argv) {
	namespace logger = levk::logger;
	auto instance = logger::Instance{};
	try {
		assert(argc > 0);
		static constexpr std::string_view const data_uris[] = {"data", "demo/data"};
		levk::run(levk::find_directory(argv[0], data_uris));
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
