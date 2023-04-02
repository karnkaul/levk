#include <imgui.h>
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

struct TestDrawable : ui::Primitive {
	using ui::Primitive::Primitive;

	void tick(Input const& input, Time dt) override {
		View::tick(input, dt);
		if (world_frame().contains(input.cursor)) {
			tint() = red_v;
			if (input.is_pressed(MouseButton::e1)) { set_destroyed(); }
		} else {
			tint() = white_v;
		}
		set_quad(QuadCreateInfo{.size = frame().extent()});
	}
};

struct LoadRequest {
	std::variant<Uri<Mesh>, Uri<Scene>> to_load{};
	std::unique_ptr<AsyncTask<void>> loader{};

	static LoadRequest make(NotNull<AssetProviders const*> asset_providers, Uri<Mesh> uri) {
		auto ret = LoadRequest{};
		auto list = AssetList{};
		list.meshes.insert(uri);
		ret.loader = std::make_unique<AssetListLoader>(asset_providers, std::move(list));
		ret.to_load = std::move(uri);
		ret.loader->run();
		return ret;
	}

	static LoadRequest make(NotNull<AssetProviders const*> asset_providers, Uri<Scene> uri) {
		auto ret = LoadRequest{};
		ret.loader = std::make_unique<AssetListLoader>(asset_providers, asset_providers->build_asset_list(uri));
		ret.to_load = std::move(uri);
		ret.loader->run();
		return ret;
	}

	explicit operator bool() const { return loader != nullptr; }

	void post_load(SceneManager& scene_manager, AssetProviders const& asset_providers) {
		auto const visitor = Visitor{
			[&scene_manager](Uri<Scene> const& uri) { scene_manager.load(uri); },
			[&scene_manager, &asset_providers](Uri<Mesh> const& uri) {
				switch (asset_providers.mesh_type(uri)) {
				case MeshType::eNone: break;
				case MeshType::eSkinned: scene_manager.active_scene().load_into_tree(Uri<SkinnedMesh>{uri}); break;
				case MeshType::eStatic: scene_manager.active_scene().load_into_tree(Uri<StaticMesh>{uri}); break;
				}
			},
		};
		std::visit(visitor, to_load);
		loader.reset();
	}
};

struct Editor : Runtime {
	static Engine::CreateInfo make_eci() {
		auto rdci = RenderDevice::CreateInfo{
			.validation = true,
			.vsync = Vsync::eOff,
		};
		return Engine::CreateInfo{
			.render_device_create_info = rdci,
		};
	}

	FreeCam free_cam{};
	LoadRequest load_request{};

	MainMenu main_menu{};
	imcpp::SceneGraph scene_graph{};
	imcpp::AssetInspector asset_inspector{};
	imcpp::EngineStatus engine_status{};
	imcpp::LogDisplay log_display{};

	struct {
		Ptr<ui::View> view{};
	} test{};

	Editor(std::string_view data_path) : Runtime(std::make_unique<DiskVfs>(data_path), make_eci()) {}

	void setup() override {
		context().active_scene().camera.transform.set_position({0.0f, 0.0f, 5.0f});
		// context().active_scene().load_into_tree(Uri<StaticMesh>{"Suzanne/Suzanne.mesh_0.json"});
		// context().scene_manager.get().load("Lantern/Lantern.scene.json");

		auto* font = m_context.asset_providers.get().ascii_font().load("fonts/Vera.ttf");
		auto drawable = std::make_unique<TestDrawable>(m_context.engine.get().render_device());
		test.view = m_context.active_scene().ui_root.add_sub_view(std::move(drawable));
		if (font) {
			auto text = std::make_unique<ui::Text>(font);
			text->n_anchor.x = 0.5f;
			text->set_string("hi");
			test.view->add_sub_view(std::move(text));
		}

		free_cam.window = &context().window();

		auto file_menu = [&] {
			// if (ImGui::MenuItem("Save", nullptr, false, !context().active_scene().empty())) { save_scene(); }
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
				if (path.extension() == ".json") {
					if (auto uri = m_context.data_source().trim_to_uri(drop)) {
						auto const asset_type = m_context.asset_providers.get().asset_type(uri);
						if (asset_type == "mesh") {
							load_request = LoadRequest::make(&m_context.asset_providers.get(), Uri<Mesh>{uri});
						} else if (asset_type == "scene") {
							load_request = LoadRequest::make(&m_context.asset_providers.get(), Uri<Scene>{uri});
						}
					}
					break;
				}
			}
		}

		if (load_request) {
			if (!ImGui::IsPopupOpen("Loading")) { imcpp::Modal::open("Loading"); }
			if (auto loading = imcpp::Modal{"Loading"}) {
				ImGui::Text("%.*s", static_cast<int>(load_request.loader->status().size()), load_request.loader->status().data());
				ImGui::ProgressBar(load_request.loader->progress());
				bool ready = load_request.loader->ready();
				if (ready) {
					load_request.post_load(m_context.scene_manager.get(), m_context.asset_providers.get());
					loading.close_current();
				}
			}
		}

		if (!context().active_scene().ui_root.contains(test.view)) { test.view = {}; }
		if (test.view) {
			auto dxy = glm::vec2{};
			if (frame.state.input.is_held(Key::eW)) { dxy.y += 1.0f; }
			if (frame.state.input.is_held(Key::eA)) { dxy.x -= 1.0f; }
			if (frame.state.input.is_held(Key::eS)) { dxy.y -= 1.0f; }
			if (frame.state.input.is_held(Key::eD)) { dxy.x += 1.0f; }
			if (std::abs(dxy.x) > 0.0f || std::abs(dxy.y) > 0.0f) {
				dxy = 1000.0f * glm::normalize(dxy) * frame.dt.count();
				test.view->set_position(test.view->frame().centre() += dxy);
			}
		}

		free_cam.tick(context().active_scene().camera.transform, frame.state.input, frame.dt);

		if (frame.state.input.chord(Key::eW, Key::eLeftControl)) { context().shutdown(); }

		auto result = main_menu.display_menu();
		switch (result.action) {
		case MainMenu::Action::eExit: m_context.shutdown(); break;
		default: break;
		}

		main_menu.display_windows();
	}
};
} // namespace
} // namespace levk

int main([[maybe_unused]] int argc, char** argv) {
	assert(argc > 0);
	static constexpr std::string_view const data_uris[] = {"data", "editor/data"};
	try {
		auto const data_path = levk::find_directory(argv[0], data_uris);
		return levk::Editor{data_path}.run();
	} catch (...) { return EXIT_FAILURE; }
}
