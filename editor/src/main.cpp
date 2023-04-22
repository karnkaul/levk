#include <imgui.h>
#include <gltf_import_wizard.hpp>
#include <levk/asset/asset_io.hpp>
#include <levk/asset/asset_list_loader.hpp>
#include <levk/context.hpp>
#include <levk/engine.hpp>
#include <levk/graphics/render_device.hpp>
#include <levk/imcpp/asset_inspector.hpp>
#include <levk/imcpp/engine_status.hpp>
#include <levk/imcpp/log_display.hpp>
#include <levk/imcpp/scene_graph.hpp>
#include <levk/level/level.hpp>
#include <levk/runtime.hpp>
#include <levk/scene/collider_aabb.hpp>
#include <levk/scene/freecam_controller.hpp>
#include <levk/scene/shape_renderer.hpp>
#include <levk/scene/skeleton_controller.hpp>
#include <levk/scene/skinned_mesh_renderer.hpp>
#include <levk/service.hpp>
#include <levk/ui/primitive.hpp>
#include <levk/ui/text.hpp>
#include <levk/util/async_task.hpp>
#include <levk/util/error.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/visitor.hpp>
#include <levk/vfs/disk_vfs.hpp>
#include <levk/window/window.hpp>
#include <main_menu.hpp>
#include <filesystem>
#include <pfd/portable-file-dialogs.hpp>

namespace levk {
namespace {
namespace fs = std::filesystem;

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
		set_quad(Quad{.size = frame().extent()});
	}
};

struct LoadRequest {
	std::variant<Uri<Mesh>, Uri<Level>> to_load{};
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

	static LoadRequest make(NotNull<AssetProviders const*> asset_providers, Uri<Level> uri, ThreadPool& thread_pool) {
		auto ret = LoadRequest{};
		ret.loader = std::make_unique<AssetListLoader>(asset_providers, asset_providers->build_asset_list(uri));
		ret.to_load = std::move(uri);
		ret.loader->run(thread_pool);
		return ret;
	}

	explicit operator bool() const { return loader != nullptr; }

	Uri<Scene> post_load(SceneManager& scene_manager) {
		auto ret = Uri<Scene>{};
		auto const visitor = Visitor{
			[&scene_manager, &ret](Uri<Level> const& uri) {
				if (scene_manager.load(uri)) { ret = uri; }
			},
			[&scene_manager](Uri<Mesh> const& uri) { scene_manager.load_and_spawn(uri); },
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
		Uri<Level> to_load{"Lantern/Lantern.level_0.json"};
		Id<Entity> entity{};
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

		test.entity = spawn({.name = "shape"}).id();
		auto& cube_entity = get_entity(test.entity);
		cube_entity.attach(std::make_unique<ShapeRenderer>());
		cube_entity.attach(std::make_unique<ColliderAabb>());

		{

			auto& cube_entity = spawn({.name = "shape1"});
			cube_entity.attach(std::make_unique<ShapeRenderer>());
			cube_entity.attach(std::make_unique<ColliderAabb>());
			cube_entity.transform().set_position({-2.0f, 0.0f, 0.0f});
		}

		auto& freecam = spawn({.name = "Freecam"});
		freecam.attach(std::make_unique<FreecamController>());
		freecam.transform().set_position({0.0f, 0.0f, 5.0f});
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
			test.load_request = {};
		};
		update_and_draw(test.load_request, on_ready);
	}

	void clear() override {
		Scene::clear();
		test.primitive = {};
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

	LoadRequest load_request{};

	MainMenu main_menu{};
	imcpp::SceneGraph scene_graph{};
	imcpp::AssetInspector asset_inspector{};
	imcpp::EngineStatus engine_status{};
	imcpp::LogDisplay log_display{};
	std::optional<imcpp::GltfImportWizard> gltf_import_wizard{};

	struct {
		std::optional<pfd::open_file> open_file{};
		std::optional<pfd::save_file> save_file{};
		std::optional<pfd::select_folder> select_folder{};

		bool active() const { return open_file || save_file || select_folder; }
	} dialogs{};

	Uri<Level> level_uri{};

	Logger log{"Editor"};

	Editor(std::string_view data_path) : Runtime(std::make_unique<DiskVfs>(data_path), make_eci()) {}

	void setup() override {
		if constexpr (debug_v) { pfd::settings::verbose(true); }
		if (pfd::settings::available()) { log.debug("Portable File Dialogs available"); }

		set_window_title();
		context().render_device().set_clear(Rgba::from({0.05f, 0.05f, 0.05f, 1.0f}));
		context().scene_manager.get().activate(std::make_unique<TestScene>());

		auto new_level = [this] {
			context().active_scene().clear();
			level_uri = {};
			set_window_title();
		};

		auto open_file = [this] {
			static auto const pfd_filters = std::vector<std::string>{
				"JSON Asset",
				"*.json",
			};
			dialogs.open_file = pfd::open_file{
				"Open Asset",
				std::string{m_data_source->mount_point()},
				pfd_filters,
			};
		};

		auto change_mount_point = [this] {
			dialogs.select_folder = pfd::select_folder{
				"Select Mount Point",
				std::string{m_data_source->mount_point()},
			};
		};

		auto save_as = [this] {
			auto uri = level_uri;
			if (uri.is_empty()) { uri = "unnamed.level.json"; }
			static auto const pfd_filters = std::vector<std::string>{
				"Level Files",
				"*.json",
				"All Files",
				"*",
			};
			dialogs.save_file = pfd::save_file{
				uri.value(),
				std::string{m_data_source->mount_point()},
				pfd_filters,
			};
		};

		auto file_menu = [=, this] {
			if (ImGui::MenuItem("New Level")) { new_level(); }
			if (!dialogs.active() && ImGui::MenuItem("Open...")) { open_file(); }
			if (level_uri && ImGui::MenuItem("Save")) { export_level(); }
			if (!dialogs.active() && ImGui::MenuItem("Save As...")) { save_as(); }
			if (!dialogs.active() && ImGui::MenuItem("Change Mount Point...")) { change_mount_point(); }
			ImGui::Separator();
			if (ImGui::MenuItem("Exit")) { m_context.shutdown(); }
		};

		main_menu.menus.lists.push_back(MainMenu::List{.label = "File", .callback = file_menu});

		main_menu.menus.window = {
			imcpp::EditorWindow{.label = "Scene", .draw_to = [&](imcpp::OpenWindow w) { scene_graph.draw_to(w, context().active_scene()); }},
			imcpp::EditorWindow{.label = "Resources", .draw_to = [&](imcpp::OpenWindow w) { asset_inspector.draw_to(w, m_context.asset_providers.get()); }},
			imcpp::EditorWindow{.label = "Engine",
								.init_size = {350.0f, 250.0f},
								.draw_to = [&](imcpp::OpenWindow w) { engine_status.draw_to(w, m_context.engine.get(), m_delta_time.value); }},
			imcpp::EditorWindow{.label = "Log", .init_size = {600.0f, 300.0f}, .draw_to = [&](imcpp::OpenWindow w) { log_display.draw_to(w); }},
		};
		if constexpr (debug_v) {
			main_menu.menus.window.push_back(MainMenu::Separator{});
			main_menu.menus.window.push_back(MainMenu::Custom{.label = "Dear ImGui Demo", .draw = [](bool& show) { ImGui::ShowDemoWindow(&show); }});
		}
		Logger::attach(log_display);
	}

	void tick(Time) final {
		auto const& state = m_context.engine.get().window().state();
		if (!load_request && !gltf_import_wizard && !state.drops.empty()) {
			for (auto const& drop : state.drops) {
				auto path = fs::path{drop};
				auto const extension = path.extension();
				if (extension == ".gltf") {
					gltf_import_wizard.emplace(path.string().c_str(), std::string{m_context.data_source().mount_point()});
					break;
				} else if (extension == ".json") {
					if (auto uri = m_context.data_source().trim_to_uri(path.generic_string())) { make_load_request(uri); }
					break;
				}
			}
		}

		if (gltf_import_wizard) {
			auto result = gltf_import_wizard->update();
			if (result.inactive) {
				gltf_import_wizard.reset();
			} else if (result) {
				if (result.should_load) { make_load_request(result.uri, result.type); }
				gltf_import_wizard.reset();
			}
		}

		if (dialogs.select_folder && dialogs.select_folder->ready()) {
			auto result = dialogs.select_folder->result();
			dialogs.select_folder.reset();
			if (!result.empty()) {
				assert(fs::is_directory(result));
				auto const dst_dir = fs::path{result} / "shaders";
				for (auto const& entry : fs::directory_iterator{fs::path{m_data_source->mount_point()} / "shaders"}) {
					auto const extension = entry.path().extension();
					auto const dst = dst_dir / entry.path().filename();
					if (extension == ".vert" || extension == ".frag") {
						fs::remove(dst);
						fs::copy(entry, dst);
					}
				}
				g_logger.info("Shaders copied to new mount point [{}]", result);
				static_cast<DiskVfs*>(m_data_source.get().get())->change_mount_point(result);
			}
		}

		if (dialogs.open_file && dialogs.open_file->ready()) {
			auto result = dialogs.open_file->result();
			dialogs.open_file.reset();
			if (!result.empty()) {
				if (auto uri = m_context.data_source().trim_to_uri(result.front())) { make_load_request(uri); }
			}
		}

		if (dialogs.save_file && dialogs.save_file->ready()) {
			auto result = dialogs.save_file->result();
			dialogs.save_file.reset();
			if (auto uri = m_data_source->trim_to_uri(result)) {
				level_uri = uri;
				export_level();
			}
		}

		auto const on_request_loaded = [&] {
			if (auto uri = load_request.post_load(m_context.scene_manager.get())) { set_window_title(); }
		};
		update_and_draw(load_request, on_request_loaded);

		if (state.input.chord(Key::eW, Key::eLeftControl)) { context().shutdown(); }

		auto result = main_menu.display_menu();
		switch (result.action) {
		case MainMenu::Action::eExit: m_context.shutdown(); break;
		default: break;
		}

		main_menu.display_windows();
	}

	void make_load_request(Uri<> const& uri, asset::Type type = asset::Type::eUnknown) {
		if (type == asset::Type::eUnknown) { type = context().asset_providers.get().asset_type(uri); }
		switch (type) {
		case asset::Type::eMesh:
			load_request = LoadRequest::make(&m_context.asset_providers.get(), Uri<Mesh>{uri}, m_context.engine.get().thread_pool());
			break;
		case asset::Type::eLevel: {
			load_request = LoadRequest::make(&m_context.asset_providers.get(), Uri<Level>{uri}, m_context.engine.get().thread_pool());
			level_uri = uri;
			break;
		}
		default: break;
		}
	}

	void export_level() {
		auto uri = level_uri;
		if (!uri) { uri = "unnamed.level.json"; }
		auto level = context().scene_manager.get().active_scene().export_level();
		auto json = dj::Json{};
		asset::to_json(json, level);
		auto* disk_vfs = dynamic_cast<DiskVfs const*>(&context().data_source());
		if (!disk_vfs || !disk_vfs->write_json(json, uri)) { return; }
		log.debug("Level exported {}", uri.value());
		set_window_title();
	}

	void set_window_title() const {
		auto title = fmt::format("levk Editor");
		if (level_uri) { fmt::format_to(std::back_inserter(title), "- {}", level_uri.value()); }
		context().window().set_title(title.c_str());
	}
};
} // namespace
} // namespace levk

int main(int argc, char** argv) {
	assert(argc > 0);
	try {
		static constexpr std::array<std::string_view, 2> patterns{"data", "editor/data"};
		auto dir = levk::Runtime::find_directory(argv[0], patterns);
		if (dir.empty()) { throw levk::Error{"Failed to find data"}; }
		levk::Editor{dir}.run();
	} catch (std::exception const& e) {
		levk::g_logger.error("Fatal error: {}", e.what());
		return EXIT_FAILURE;
	}
}
