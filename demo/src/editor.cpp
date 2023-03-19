#include <imgui.h>
#include <editor.hpp>
#include <levk/asset/gltf_importer.hpp>
#include <filesystem>

namespace levk {
namespace {
namespace fs = std::filesystem;

enum class LoadType { eStaticMesh, eSkinnedMesh, eScene };

struct AssetListLoader : AsyncTask<Uri<>> {
	AssetList list;
	AssetProviders& providers;
	Uri<> ret;
	LoadType load_type{};

	AssetListLoader(AssetList list, AssetProviders& providers, Uri<> ret, LoadType load_type)
		: list(std::move(list)), providers(providers), ret(std::move(ret)), load_type(load_type) {}

	AssetListLoader(Uri<Scene> const& uri, Editor& editor)
		: AssetListLoader(editor.context().providers.get().build_asset_list(uri), editor.context().providers.get(), uri, LoadType::eScene) {}
	AssetListLoader(Uri<StaticMesh> const& uri, Editor& editor)
		: AssetListLoader(for_mesh(uri), editor.context().providers.get(), uri, LoadType::eStaticMesh) {}
	AssetListLoader(Uri<SkinnedMesh> const& uri, Editor& editor)
		: AssetListLoader(for_mesh(uri), editor.context().providers.get(), uri, LoadType::eSkinnedMesh) {}

	static AssetList for_mesh(Uri<asset::Mesh3D> uri) {
		auto ret = AssetList{};
		ret.meshes.insert(std::move(uri));
		return ret;
	}

	Uri<> execute() override {
		auto const total = list.shaders.size() + list.textures.size() + list.materials.size() + list.meshes.size() + list.skeletons.size();
		auto done = std::size_t{};
		auto increment_done = [&] {
			++done;
			this->set_progress(static_cast<float>(done) / static_cast<float>(total));
		};
		this->set_status("Loading Shaders");
		for (auto const& uri : list.shaders) {
			providers.shader().load(uri);
			increment_done();
		}
		this->set_status("Loading Textures");
		for (auto const& uri : list.textures) {
			providers.texture().load(uri);
			increment_done();
		}
		this->set_status("Loading Materials");
		for (auto const& uri : list.materials) {
			providers.material().load(uri);
			increment_done();
		}
		this->set_status("Loading Skeletons");
		for (auto const& uri : list.skeletons) {
			providers.skeleton().load(uri);
			increment_done();
		}
		this->set_status("Loading Meshes");
		for (auto const& uri : list.meshes) {
			switch (providers.mesh_type(uri)) {
			case MeshType::eStatic: providers.static_mesh().load(uri); break;
			case MeshType::eSkinned: providers.skinned_mesh().load(uri); break;
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

Editor::Editor(std::unique_ptr<DiskVfs> vfs)
	: Runtime(*vfs, vfs->uri_monitor(), DesktopContextFactory{}), m_free_cam{&m_context.engine.get().window()}, m_vfs(std::move(vfs)) {
	auto file_menu = [&] {
		if (ImGui::MenuItem("Save", nullptr, false, !active_scene().empty())) { save_scene(); }
		ImGui::Separator();
		if (ImGui::MenuItem("Exit")) { m_context.shutdown(); }
	};

	m_main_menu.menus.lists.push_back(MainMenu::List{.label = "File", .callback = file_menu});

	m_main_menu.menus.window = {
		imcpp::EditorWindow{.label = "Scene", .draw_to = [&](imcpp::OpenWindow w) { m_scene_graph.draw_to(w, active_scene()); }},
		imcpp::EditorWindow{.label = "Resources", .draw_to = [&](imcpp::OpenWindow w) { m_asset_inspector.draw_to(w, m_context.providers.get()); }},
		imcpp::EditorWindow{
			.label = "Engine", .init_size = {350.0f, 250.0f}, .draw_to = [&](imcpp::OpenWindow w) { m_engine_status.draw_to(w, m_context.engine.get()); }},
		imcpp::EditorWindow{.label = "Log", .init_size = {600.0f, 300.0f}, .draw_to = [&](imcpp::OpenWindow w) { m_log_display.draw_to(w); }},
	};
	if constexpr (debug_v) {
		m_main_menu.menus.window.push_back(MainMenu::Separator{});
		m_main_menu.menus.window.push_back(MainMenu::Custom{.label = "Dear ImGui Demo", .draw = [](bool& show) { ImGui::ShowDemoWindow(&show); }});
	}

	{
		if (auto* font = m_context.providers.get().ascii_font().load("fonts/Vera.ttf")) {
			m_test.primitive.emplace(m_context.engine.get().render_device());
			m_test.primitive->height = TextHeight{72u};
			m_test.primitive->update("hello", *font);
		}
	}
}

void Editor::save_scene() const {
	auto uri = m_scene_uri ? m_scene_uri : Uri<Scene>{"unnamed.scene.json"};
	auto json = dj::Json{};
	active_scene().serialize(json);
	if (!m_vfs->write_json(json, uri)) { return; }
	logger::debug("Scene saved {}", uri.value());
}

void Editor::tick(Frame const& frame) {
	if (frame.state.focus_changed() && frame.state.is_in_focus()) { m_context.providers.get().uri_monitor().dispatch_modified(); }
	if (frame.state.input.chord(Key::eW, Key::eLeftControl) || frame.state.input.chord(Key::eW, Key::eRightControl)) { m_context.shutdown(); }

	for (auto const& drop : frame.state.drops) {
		auto path = fs::path{drop};
		if (path.extension() == ".gltf") {
			auto export_path = path.filename().stem();
			m_gltf_importer.emplace(drop.c_str(), std::string{m_context.data_source().mount_point()});
			break;
		}
		if (path.extension() == ".json") {
			if (auto uri = m_context.data_source().trim_to_uri(drop)) {
				auto const asset_type = m_context.providers.get().asset_type(uri);
				if (asset_type == "mesh") {
					if (m_context.providers.get().mesh_type(uri) == MeshType::eSkinned) {
						active_scene().load_into_tree(Uri<SkinnedMesh>{uri});
					} else {
						active_scene().load_into_tree(Uri<StaticMesh>{uri});
					}
				} else if (asset_type == "scene") {
					m_load = std::make_unique<AssetListLoader>(Uri<Scene>{std::move(uri)}, *this);
					m_load->run();
				}
			}
			break;
		}
	}

	active_scene().tick(frame.dt);
	m_free_cam.tick(active_scene().camera.transform, frame.state.input, frame.dt);

	// test code
	if (frame.state.input.chord(Key::eS, Key::eLeftControl)) { save_scene(); }
	// test code

	auto result = m_main_menu.display_menu();
	switch (result.action) {
	case MainMenu::Action::eExit: m_context.shutdown(); break;
	default: break;
	}

	m_main_menu.display_windows();

	if (m_load) {
		if (!ImGui::IsPopupOpen("Loading")) { imcpp::Modal::open("Loading"); }
		if (auto loading = imcpp::Modal{"Loading"}) {
			ImGui::Text("%.*s", static_cast<int>(m_load->status().size()), m_load->status().data());
			ImGui::ProgressBar(m_load->progress());
			if (m_load->ready()) {
				switch (dynamic_cast<AssetListLoader&>(*m_load).load_type) {
				case LoadType::eStaticMesh: active_scene().load_into_tree(Uri<StaticMesh>{m_load->get()}); break;
				case LoadType::eSkinnedMesh: active_scene().load_into_tree(Uri<SkinnedMesh>{m_load->get()}); break;
				case LoadType::eScene: {
					m_scene_uri = m_load->get();
					m_scene_manager.get().load(m_scene_uri);
					break;
				}
				default: break;
				}
				loading.close_current();
				m_load.reset();
			}
		}
	}

	if (m_gltf_importer) {
		auto result = m_gltf_importer->update();
		if (result.inactive) {
			m_gltf_importer.reset();
		} else if (result && result.should_load) {
			if (result.scene) {
				m_load = std::make_unique<AssetListLoader>(std::move(result.scene), *this);
			} else if (result.mesh) {
				m_load = std::visit([&](auto const& uri) { return std::make_unique<AssetListLoader>(uri, *this); }, *result.mesh);
			}
			m_load->run();
			m_gltf_importer.reset();
		}
	}
}

void Editor::render() {
	struct TestRenderer : Renderer {
		Editor& editor;

		TestRenderer(Editor& editor) : editor(editor) {}

		void render_3d(DrawList& out) const final { editor.active_scene().render_3d(out); }

		void render_ui(DrawList& out) const final {
			editor.active_scene().render_ui(out);
			if (editor.m_test.primitive) {
				editor.m_test.transforms[0].set_position({-100.0f, 0.0f, -5.0f});
				editor.m_test.transforms[1].set_position({100.0f, 100.0f, 0.0f});
				auto const instances = DrawList::Instanced{.instances = editor.m_test.transforms};
				out.add(*editor.m_test.primitive->primitive, editor.m_test.primitive->material, instances);
			}
		}
	};
	m_context.engine.get().render_device().clear_colour = Rgba::from({0.1f, 0.1f, 0.1f, 1.0f});
	m_context.engine.get().render(TestRenderer{*this}, m_context.providers.get(), active_scene().camera, active_scene().lights);
}
} // namespace levk
