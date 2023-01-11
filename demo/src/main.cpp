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
#include <levk/util/visitor.hpp>
#include <filesystem>

#include <levk/imcpp/engine_inspector.hpp>
#include <levk/imcpp/log_renderer.hpp>
#include <levk/imcpp/resource_inspector.hpp>
#include <levk/imcpp/scene_inspector.hpp>
#include <main_menu.hpp>

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

	auto resource_inspector = imcpp::ResourceInspector{};
	auto scene_inspector = imcpp::SceneInspector{};
	auto engine_inspector = imcpp::EngineInspector{};
	auto log_renderer = imcpp::LogRenderer{};
	auto main_menu = MainMenu{};

	main_menu.windows = {
		{.label = "Scene"}, {.label = "Resources"}, {.label = "Engine", .init_size = {300.0f, 300.0f}}, {.label = "Log", .init_size = {600.0f, 300.0f}},
		{.label = "--"},
	};
	if constexpr (debug_v) { main_menu.windows.push_back({.label = "Dear ImGui Demo"}); }

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

		// test code
		if (frame.state.input.chord(Key::eS, Key::eLeftControl)) {
			auto json = dj::Json{};
			scene->serialize(json);
			auto filename = "test_scene.json";
			json.to_file((data_path / filename).string().c_str());
			logger::debug("Scene saved to {}", filename);
		}
		// test code

		auto result = main_menu.display();
		switch (result.action) {
		case MainMenu::Action::eExit: engine.shutdown(); break;
		default: break;
		}

		main_menu.windows[0].display([&](imcpp::OpenWindow w) { scene_inspector.inspect(w, *scene); });
		main_menu.windows[1].display([&](imcpp::OpenWindow w) { resource_inspector.inspect(w, Service<Resources>::locate()); });
		main_menu.windows[2].display([&](imcpp::OpenWindow w) { engine_inspector.inspect(w, Service<Engine>::locate(), frame.dt); });
		main_menu.windows[3].display([&](imcpp::OpenWindow w) { log_renderer.display(w); });

		if constexpr (debug_v) {
			if (bool& show = main_menu.windows[5].show.value) { ImGui::ShowDemoWindow(&show); }
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
