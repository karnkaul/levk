#include <imgui.h>
#include <gltf_import_wizard.hpp>
#include <levk/asset/asset_list_loader.hpp>
#include <levk/graphics/shader.hpp>
#include <levk/graphics/texture.hpp>
#include <levk/imcpp/asset_inspector.hpp>
#include <levk/imcpp/common.hpp>
#include <levk/imcpp/engine_status.hpp>
#include <levk/imcpp/log_display.hpp>
#include <levk/imcpp/reflector.hpp>
#include <levk/imcpp/scene_graph.hpp>
#include <levk/runtime.hpp>
#include <levk/ui/text.hpp>
#include <levk/util/error.hpp>
#include <levk/util/fixed_string.hpp>
#include <levk/util/visitor.hpp>
#include <levk/vfs/disk_vfs.hpp>
#include <main_menu.hpp>
#include <cassert>
#include <filesystem>

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
			dxy.y = -dxy.y;
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

struct TestUiPrimitive : ui::Primitive {
	using ui::Primitive::Primitive;

	bool clicked{};

	void tick(Input const& input, Time dt) override {
		View::tick(input, dt);
		if (world_frame().contains(input.cursor)) {
			tint() = yellow_v;
			if (input.is_pressed(MouseButton::e1)) { clicked = true; }
		} else {
			tint() = white_v;
		}
		set_quad(QuadCreateInfo{.size = frame().extent()});
	}
};

struct LoadRequest {
	std::variant<Uri<Mesh>, Uri<Scene>> to_load{};
	std::unique_ptr<AsyncTask<void>> loader{};

	static LoadRequest make(NotNull<AssetProviders const*> asset_providers, Uri<Mesh> uri, ThreadPool& thread_pool) {
		auto ret = LoadRequest{};
		auto list = AssetList{};
		list.meshes.insert(uri);
		ret.loader = std::make_unique<AssetListLoader>(asset_providers, std::move(list));
		ret.to_load = std::move(uri);
		ret.loader->run(thread_pool);
		return ret;
	}

	static LoadRequest make(NotNull<AssetProviders const*> asset_providers, Uri<Scene> uri, ThreadPool& thread_pool) {
		auto ret = LoadRequest{};
		ret.loader = std::make_unique<AssetListLoader>(asset_providers, asset_providers->build_asset_list(uri));
		ret.to_load = std::move(uri);
		ret.loader->run(thread_pool);
		return ret;
	}

	explicit operator bool() const { return loader != nullptr; }

	Uri<Scene> post_load(SceneManager& scene_manager, AssetProviders const& asset_providers) {
		auto ret = Uri<Scene>{};
		auto const visitor = Visitor{
			[&scene_manager, &ret](Uri<Scene> const& uri) {
				if (scene_manager.load(uri)) { ret = uri; }
			},
			[&scene_manager, &asset_providers](Uri<Mesh> const& uri) {
				switch (asset_providers.mesh_type(uri)) {
				case MeshType::eNone: break;
				case MeshType::eSkinned: scene_manager.load_into_tree(Uri<SkinnedMesh>{uri}); break;
				case MeshType::eStatic: scene_manager.load_into_tree(Uri<StaticMesh>{uri}); break;
				}
			},
		};
		std::visit(visitor, to_load);
		loader.reset();
		return ret;
	}
};

template <typename F>
void update_and_draw(LoadRequest& load_request, F on_ready) {
	if (load_request) {
		if (!ImGui::IsPopupOpen("Loading")) { imcpp::Modal::open("Loading"); }
		if (auto loading = imcpp::Modal{"Loading"}) {
			ImGui::Text("%.*s", static_cast<int>(load_request.loader->status().size()), load_request.loader->status().data());
			ImGui::ProgressBar(load_request.loader->progress());
			if (load_request.loader->ready()) {
				on_ready();
				loading.close_current();
			}
		}
	}
}

struct TestScene : Scene {
	struct {
		Ptr<TestUiPrimitive> primitive{};
		LoadRequest load_request{};
		Uri<Scene> to_load{"Lantern/Lantern.scene_0.json"};
	} test{};

	Logger log{"TestScene"};

	void setup() override {
		log.debug("TestScene::setup()");
		camera.transform.set_position({0.0f, 0.0f, 5.0f});
		lights.primary.direction = Transform::look_at({-1.0f, -1.0f, -1.0f}, {});
		// Service<SceneManager>::locate().load_into_tree(Uri<StaticMesh>{"Suzanne/Suzanne.mesh_0.json"});

		auto* font = Service<AssetProviders>::locate().ascii_font().load("fonts/Vera.ttf");
		auto ui_primitive = std::make_unique<TestUiPrimitive>();
		ui_primitive->set_extent({200.0f, 75.0f});
		test.primitive = ui_primitive.get();
		ui_root.add_sub_view(std::move(ui_primitive));
		if (font) {
			auto text = std::make_unique<ui::Text>(font);
			text->set_string("click");
			text->tint() = black_v;
			test.primitive->add_sub_view(std::move(text));
		}
	}

	void tick(Time dt) override {
		Scene::tick(dt);
		if (input().chord(Key::eE, Key::eLeftControl)) {}

		if (test.primitive) {
			auto dxy = glm::vec2{};
			if (input().is_held(Key::eW)) { dxy.y += 1.0f; }
			if (input().is_held(Key::eA)) { dxy.x -= 1.0f; }
			if (input().is_held(Key::eS)) { dxy.y -= 1.0f; }
			if (input().is_held(Key::eD)) { dxy.x += 1.0f; }
			if (std::abs(dxy.x) > 0.0f || std::abs(dxy.y) > 0.0f) {
				dxy = 1000.0f * glm::normalize(dxy) * dt.count();
				test.primitive->set_position(test.primitive->frame().centre() += dxy);
			}

			if (test.primitive->clicked) {
				if (!test.load_request && Service<SceneManager>().locate().data_source().contains(test.to_load)) {
					test.load_request = LoadRequest::make(&Service<AssetProviders>::locate(), test.to_load, Service<Engine>::locate().thread_pool());
				}
				test.primitive->set_destroyed();
				test.primitive = {};
			}
		}
		auto on_ready = [this] {
			if (Service<SceneManager>::locate().load(test.to_load)) {
				log.debug("loading [{}]", test.to_load.value());
			} else {
				log.debug("Could not load [{}]", test.to_load.value());
			}
		};
		update_and_draw(test.load_request, on_ready);
	}
};

struct Editor : Runtime {
	static Engine::CreateInfo make_eci() {
		auto rdci = RenderDevice::CreateInfo{
			.validation = debug_v,
			.vsync = Vsync::eOff,
		};
		return Engine::CreateInfo{
			.render_device_create_info = rdci,
		};
	}

	FreeCam free_cam{};
	LoadRequest load_request{};

	Uri<Scene> scene_uri{"unnamed.scene.json"};
	MainMenu main_menu{};
	imcpp::SceneGraph scene_graph{};
	imcpp::AssetInspector asset_inspector{};
	imcpp::EngineStatus engine_status{};
	imcpp::LogDisplay log_display{};
	std::optional<imcpp::GltfImportWizard> gltf_import_wizard{};

	Logger log{"Editor"};

	Editor(std::string_view data_path) : Runtime(std::make_unique<DiskVfs>(data_path), make_eci()) {}

	void setup() override {
		set_window_title();
		context().render_device().set_clear(Rgba::from({0.05f, 0.05f, 0.05f, 1.0f}));
		context().scene_manager.get().set_active<TestScene>();

		free_cam.window = &context().window();

		auto file_menu = [&] {
			if (ImGui::MenuItem("Save", nullptr, false, !context().active_scene().empty())) { save_scene(); }
			ImGui::Separator();
			if (ImGui::MenuItem("Exit")) { m_context.shutdown(); }
		};

		main_menu.menus.lists.push_back(MainMenu::List{.label = "File", .callback = file_menu});

		main_menu.menus.window = {
			imcpp::EditorWindow{.label = "Scene", .draw_to = [&](imcpp::OpenWindow w) { scene_graph.draw_to(w, context().active_scene()); }},
			imcpp::EditorWindow{.label = "Resources", .draw_to = [&](imcpp::OpenWindow w) { asset_inspector.draw_to(w, m_context.asset_providers.get()); }},
			imcpp::EditorWindow{
				.label = "Engine", .init_size = {350.0f, 250.0f}, .draw_to = [&](imcpp::OpenWindow w) { engine_status.draw_to(w, m_context.engine.get()); }},
			imcpp::EditorWindow{.label = "Log", .init_size = {600.0f, 300.0f}, .draw_to = [&](imcpp::OpenWindow w) { log_display.draw_to(w); }},
		};
		if constexpr (debug_v) {
			main_menu.menus.window.push_back(MainMenu::Separator{});
			main_menu.menus.window.push_back(MainMenu::Custom{.label = "Dear ImGui Demo", .draw = [](bool& show) { ImGui::ShowDemoWindow(&show); }});
		}
	}

	void tick(Frame const& frame) final {
		if (!load_request && !frame.state.drops.empty()) {
			for (auto const& drop : frame.state.drops) {
				auto path = fs::path{drop};
				auto const extension = path.extension();
				if (extension == ".gltf") {
					gltf_import_wizard.emplace(drop.c_str(), std::string{m_context.data_source().mount_point()});
					break;
				} else if (extension == ".json") {
					if (auto uri = m_context.data_source().trim_to_uri(drop)) {
						auto const asset_type = m_context.asset_providers.get().asset_type(uri);
						if (asset_type == "mesh") {
							load_request = LoadRequest::make(&m_context.asset_providers.get(), Uri<Mesh>{uri}, m_context.engine.get().thread_pool());
						} else if (asset_type == "scene") {
							load_request = LoadRequest::make(&m_context.asset_providers.get(), Uri<Scene>{uri}, m_context.engine.get().thread_pool());
						}
					}
					break;
				}
			}
		}

		if (gltf_import_wizard) {
			auto result = gltf_import_wizard->update();
			if (result.inactive) {
				gltf_import_wizard.reset();
			} else if (result && result.should_load) {
				if (result.scene) {
					load_request = LoadRequest::make(&context().asset_providers.get(), std::move(result.scene), m_context.engine.get().thread_pool());
				} else if (result.mesh) {
					load_request = LoadRequest::make(&context().asset_providers.get(), std::move(result.mesh), m_context.engine.get().thread_pool());
				}
				gltf_import_wizard.reset();
			}
		}

		auto const on_request_loaded = [&] {
			if (auto uri = load_request.post_load(m_context.scene_manager.get(), m_context.asset_providers.get())) {
				scene_uri = std::move(uri);
				set_window_title();
			}
		};
		update_and_draw(load_request, on_request_loaded);

		free_cam.tick(context().active_scene().camera.transform, frame.state.input, frame.dt);

		if (frame.state.input.chord(Key::eW, Key::eLeftControl)) { context().shutdown(); }

		auto result = main_menu.display_menu();
		switch (result.action) {
		case MainMenu::Action::eExit: m_context.shutdown(); break;
		default: break;
		}

		main_menu.display_windows();
	}

	void save_scene() {
		auto uri = scene_uri ? scene_uri : Uri<Scene>{"unnamed.scene.json"};
		auto json = context().serializer.get().serialize(context().active_scene());
		auto* disk_vfs = dynamic_cast<DiskVfs const*>(&context().data_source());
		if (!disk_vfs || !disk_vfs->write_json(json, uri)) { return; }
		log.debug("Scene saved {}", uri.value());
		set_window_title();
	}

	void set_window_title() const {
		auto const title = fmt::format("levk Editor - {}", scene_uri.value());
		context().window().set_title(title.c_str());
	}
};
} // namespace
} // namespace levk

int main([[maybe_unused]] int argc, char** argv) {
	using namespace levk;
	assert(argc > 0);
	auto const log{Logger{"Editor"}};
	try {
		static constexpr std::string_view const data_uris[] = {"data", "editor/data"};
		auto const data_path = levk::Runtime::find_directory(argv[0], data_uris);
		Editor{data_path}.run();
	} catch (Error const& e) {
		log.error("Runtime error: {}", e.what());
		return EXIT_FAILURE;
	} catch (std::exception const& e) {
		log.error("Fatal error: {}", e.what());
		return EXIT_FAILURE;
	} catch (...) {
		log.error("Unknown error");
		return EXIT_FAILURE;
	}
	log.debug("exited successfully");
}
