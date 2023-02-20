#include <imgui.h>
#include <editor.hpp>
#include <levk/asset/asset_loader.hpp>
#include <levk/asset/gltf_importer.hpp>
#include <filesystem>

namespace levk {
namespace {
namespace fs = std::filesystem;

std::string trim_to_uri(std::string_view full, std::string_view data) {
	auto const i = full.find(data);
	if (i == std::string_view::npos) { return {}; }
	full = full.substr(i + data.size());
	while (!full.empty() && full.front() == '/') { full = full.substr(1); }
	return std::string{full};
}

enum class LoadType { eStaticMesh, eSkinnedMesh, eScene };

struct AssetListLoader : AsyncTask<Uri<>> {
	AssetList list;
	AssetLoader loader;
	Uri<> ret;
	LoadType load_type{};

	AssetListLoader(AssetList list, AssetLoader loader, Uri<> ret, LoadType load_type)
		: list(std::move(list)), loader(loader), ret(std::move(ret)), load_type(load_type) {}

	AssetListLoader(Uri<Scene> const& uri, Editor& editor)
		: AssetListLoader(AssetLoader::get_asset_list(uri, editor.reader(), Reader::eReload), make_loader(editor), uri, LoadType::eScene) {}

	AssetListLoader(Uri<StaticMesh> const& uri, Editor& editor) : AssetListLoader(for_mesh(uri), make_loader(editor), uri, LoadType::eStaticMesh) {}
	AssetListLoader(Uri<SkinnedMesh> const& uri, Editor& editor) : AssetListLoader(for_mesh(uri), make_loader(editor), uri, LoadType::eSkinnedMesh) {}

	static AssetList for_mesh(Uri<asset::Mesh> uri) {
		auto ret = AssetList{};
		ret.meshes.insert(std::move(uri));
		return ret;
	}

	static AssetLoader make_loader(Editor& editor) {
		return AssetLoader{editor.reader(), editor.context().engine.get().device(), editor.context().resources.get().render};
	}

	Uri<> execute() override {
		auto const total = list.materials.size() + list.meshes.size() + list.skeletons.size();
		auto done = std::size_t{};
		auto increment_done = [&] {
			++done;
			this->set_progress(static_cast<float>(done) / static_cast<float>(total));
		};
		this->set_status("Loading Materials");
		for (auto const& material : list.materials) {
			loader.load_material(material);
			increment_done();
		}
		this->set_status("Loading Skeletons");
		for (auto const& skeleton : list.skeletons) {
			loader.load_skeleton(skeleton);
			increment_done();
		}
		this->set_status("Loading Meshes");
		for (auto const& mesh : list.meshes) {
			switch (AssetLoader::get_mesh_type(mesh, loader.reader)) {
			case MeshType::eStatic: loader.load_static_mesh(mesh); break;
			case MeshType::eSkinned: loader.load_skinned_mesh(mesh); break;
			default: break;
			}
			increment_done();
		}
		this->set_status("Done");
		return std::move(ret);
	}
};
} // namespace

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
	: Runtime(std::make_unique<FileReader>(data_path), DesktopContextFactory{}), data_path(std::move(data_path)), free_cam{&m_context.engine.get().window()} {
	auto file_menu = [&] {
		if (ImGui::MenuItem("Save", nullptr, false, !scene.empty())) { save_scene(); }
		ImGui::Separator();
		if (ImGui::MenuItem("Exit")) { m_context.shutdown(); }
	};

	main_menu.menus.lists.push_back(MainMenu::List{.label = "File", .callback = file_menu});

	main_menu.menus.window = {
		imcpp::EditorWindow{.label = "Scene", .draw_to = [&](imcpp::OpenWindow w) { scene_graph.draw_to(w, scene); }},
		imcpp::EditorWindow{.label = "Resources", .draw_to = [&](imcpp::OpenWindow w) { resource_display.draw_to(w, Service<Resources>::locate()); }},
		imcpp::EditorWindow{
			.label = "Engine", .init_size = {350.0f, 250.0f}, .draw_to = [&](imcpp::OpenWindow w) { engine_status.draw_to(w, m_context.engine.get()); }},
		imcpp::EditorWindow{.label = "Log", .init_size = {600.0f, 300.0f}, .draw_to = [&](imcpp::OpenWindow w) { log_display.draw_to(w); }},
	};
	if constexpr (debug_v) {
		main_menu.menus.window.push_back(MainMenu::Separator{});
		main_menu.menus.window.push_back(MainMenu::Custom{.label = "Dear ImGui Demo", .draw = [](bool& show) { ImGui::ShowDemoWindow(&show); }});
	}
}

void Editor::save_scene() const {
	auto uri = scene_uri ? scene_uri : Uri<Scene>{"unnamed.scene.json"};
	if (!save(scene, uri)) { return; }
	logger::debug("Scene saved {}", uri.value());
}

void Editor::tick(Frame const& frame) {
	if (frame.state.focus_changed() && frame.state.is_in_focus()) { static_cast<FileReader*>(m_reader.get())->reload_out_of_date(); }
	if (frame.state.input.chord(Key::eW, Key::eLeftControl) || frame.state.input.chord(Key::eW, Key::eRightControl)) { m_context.shutdown(); }

	for (auto const& drop : frame.state.drops) {
		auto path = fs::path{drop};
		if (path.extension() == ".gltf") {
			auto export_path = path.filename().stem();
			gltf_importer.emplace(drop.c_str(), data_path);
			break;
		}
		if (path.extension() == ".json") {
			auto uri = trim_to_uri(fs::path{drop}.generic_string(), data_path);
			if (!uri.empty()) {
				auto const asset_type = AssetLoader::get_asset_type(drop, *m_reader);
				if (asset_type == "mesh") {
					if (AssetLoader::get_mesh_type(drop, *m_reader) == MeshType::eSkinned) {
						scene.load_into_tree(Uri<SkinnedMesh>{uri});
					} else {
						scene.load_into_tree(Uri<StaticMesh>{uri});
					}
				} else if (asset_type == "scene") {
					m_load = std::make_unique<AssetListLoader>(Uri<Scene>{std::move(uri)}, *this);
					m_load->run();
				}
			}
			break;
		}
	}

	scene.tick(frame.dt);
	free_cam.tick(scene.camera.transform, frame.state.input, frame.dt);

	// test code
	if (frame.state.input.chord(Key::eS, Key::eLeftControl)) { save_scene(); }
	// test code

	auto result = main_menu.display_menu();
	switch (result.action) {
	case MainMenu::Action::eExit: m_context.shutdown(); break;
	default: break;
	}

	main_menu.display_windows();

	if (m_load) {
		if (!ImGui::IsPopupOpen("Loading")) { imcpp::Modal::open("Loading"); }
		if (auto loading = imcpp::Modal{"Loading"}) {
			ImGui::Text("%.*s", static_cast<int>(m_load->status().size()), m_load->status().data());
			ImGui::ProgressBar(m_load->progress());
			if (m_load->ready()) {
				switch (dynamic_cast<AssetListLoader&>(*m_load).load_type) {
				case LoadType::eStaticMesh: scene.load_into_tree(Uri<StaticMesh>{m_load->get()}); break;
				case LoadType::eSkinnedMesh: scene.load_into_tree(Uri<SkinnedMesh>{m_load->get()}); break;
				case LoadType::eScene: load_into(scene, Uri<Scene>{m_load->get()}); break;
				default: break;
				}
				loading.close_current();
				m_load.reset();
			}
		}
	}

	if (gltf_importer) {
		auto result = gltf_importer->update();
		if (result.inactive) {
			gltf_importer.reset();
		} else if (result && result.should_load) {
			if (result.scene) {
				m_load = std::make_unique<AssetListLoader>(std::move(result.scene), *this);
			} else if (result.mesh) {
				m_load = std::visit([&](auto const& uri) { return std::make_unique<AssetListLoader>(uri, *this); }, *result.mesh);
			}
			m_load->run();
			gltf_importer.reset();
		}
	}
}

void Editor::render() { m_context.render(scene, clear_colour); }
} // namespace levk
