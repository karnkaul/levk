#include <imgui.h>
#include <editor.hpp>
#include <levk/asset/gltf_importer.hpp>
#include <filesystem>

namespace levk {
namespace {
namespace fs = std::filesystem;
}

void FreeCam::tick(Transform& transform, Input const& input, Time dt) {
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

bool Editor::import_gltf(char const* in_path, std::string_view dest_dir) {
	auto src = fs::path{in_path};
	auto dst = fs::path{data_path} / dest_dir;
	auto src_filename = src.filename().stem();
	auto export_path = dst / src_filename;
	auto asset_list = asset::GltfImporter::peek(in_path);

	if (!asset_list) { return {}; }

	if (asset_list.static_meshes.empty() && asset_list.skinned_meshes.empty()) {
		logger::error("No meshes found in {}\n", in_path);
		return {};
	}

	bool is_skinned{};
	auto mesh_asset = [&]() -> Ptr<asset::GltfAsset const> {
		auto const func = [](asset::GltfAsset const& asset) { return asset.index == 0; };
		if (auto it = std::find_if(asset_list.static_meshes.begin(), asset_list.static_meshes.end(), func); it != asset_list.static_meshes.end()) {
			return &*it;
		}
		if (auto it = std::find_if(asset_list.skinned_meshes.begin(), asset_list.skinned_meshes.end(), func); it != asset_list.skinned_meshes.end()) {
			is_skinned = true;
			return &*it;
		}
		return nullptr;
	}();

	auto importer = asset_list.importer(dst.string());
	if (!importer) { return {}; }

	auto mesh_uri = importer.import_mesh(*mesh_asset);
	if (mesh_uri.value().empty()) {
		logger::error("Import failed! {}\n", mesh_asset->asset_name);
		return {};
	}
	mesh_uri = (fs::path{dest_dir} / mesh_uri.value()).generic_string();

	if (is_skinned) {
		return scene.load_skinned_mesh_into_tree(mesh_uri.value());
	} else {
		return scene.load_static_mesh_into_tree(mesh_uri.value());
	}
}
} // namespace levk
