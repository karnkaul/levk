#include <imgui.h>
#include <levk/editor/common.hpp>
#include <levk/engine.hpp>
#include <levk/entity.hpp>
#include <levk/impl/desktop_window.hpp>
#include <levk/impl/vulkan_device.hpp>
#include <levk/transform_controller.hpp>
#include <levk/util/error.hpp>
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

using MeshStorage = ResourceMap<StaticMesh>;

struct Scene : GraphicsRenderer {
	MeshStorage meshes{};
	Entity::Tree tree{};

	void render(GraphicsDevice& device, Entity const& entity, glm::mat4 parent = glm::mat4{1.0f}) const {
		if (auto const* uri = entity.find<Uri<StaticMesh>>()) {
			if (auto const* mesh = meshes.find(*uri)) { device.render(*mesh, {&entity.transform, 1u}, parent); }
		}
		if (entity.children().empty()) { return; }
		parent = parent * entity.transform.matrix();
		for (auto const id : entity.children()) {
			if (auto const* child = tree.find(id)) { render(device, *child, parent); }
		}
	}

	void render_3d(GraphicsDevice& device) final {
		for (auto const& node : tree.roots()) { render(device, *tree.find(node)); }
	}

	bool walk(Entity& e) {
		auto tn = editor::TreeNode{e.name.c_str()};
		auto const id = e.id().value();
		if (ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload("entity", &id, sizeof(id));
			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget()) {
			if (auto const* payload = ImGui::AcceptDragDropPayload("entity")) {
				auto const child = *static_cast<std::size_t const*>(payload->Data);
				tree.reparent(tree.get(child), id);
				return false;
			}
			ImGui::EndDragDropTarget();
		}
		if (tn) {
			for (auto& id : e.children()) {
				if (!walk(tree.get(id))) { return false; }
			}
		}
		return true;
	}

	void render_ui(GraphicsDevice& out) final {
		if (auto w = editor::Window{"Scene"}) {
			float scale = out.info().render_scale;
			if (ImGui::DragFloat("Render Scale", &scale, 0.05f, render_scale_limit_v[0], render_scale_limit_v[1])) { out.set_render_scale(scale); }
			ImGui::Separator();
			for (auto const& e : tree.roots()) {
				if (!walk(tree.get(e))) { return; }
			}
		}
	}
};

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

		if (window && window->cursor_mode() == CursorMode::eDisabled) {
			auto const dxy = input.cursor - prev_cursor;
			if (std::abs(dxy.x) > 0.0f) { horz = glm::rotate(horz, glm::radians(-dxy.x) * look_speed, up_v); }
			if (std::abs(dxy.y) > 0.0f) { vert = glm::rotate(vert, glm::radians(-dxy.y) * look_speed, right_v); }
		}

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
	auto scene = Scene{};
	auto geometry = make_cube({1.0f, 1.0f, 1.0f});
	auto free_cam = FreeCam{&engine.window()};
	auto mesh = StaticMesh{};
	mesh.materials.push_back(LitMaterial{});
	mesh.primitives.push_back(StaticMesh::Primitive{engine.device().make_mesh_geometry(geometry), 0});
	scene.meshes.add("test_mesh", std::move(mesh));
	auto e_0 = Entity{};
	e_0.name = "e0";
	e_0.attach(Uri<StaticMesh>{"test_mesh"});
	auto& e0 = scene.tree.add(std::move(e_0));
	auto e_1 = Entity{};
	e_1.name = "e1";
	e_1.attach(Uri<StaticMesh>{"test_mesh"});
	auto& e1 = scene.tree.add(std::move(e_1), e0.id());
	e1.transform.set_position({-2.0f, 0.0f, 0.0f});
	auto e_2 = Entity{};
	e_2.name = "e2";
	e_2.attach(Uri<StaticMesh>{"test_mesh"});
	e_2.transform.set_position({2.0f, 0.0f, 0.0f});
	auto& e2 = scene.tree.add(std::move(e_2), e0.id());
	auto e_3 = Entity{};
	e_3.name = "e3";
	e_3.attach(Uri<StaticMesh>{"test_mesh"});
	auto& e3 = scene.tree.add(std::move(e_3));
	e3.transform.set_position({0.0f, 2.0f, 0.0f});

	engine.show();
	while (engine.is_running()) {
		auto frame = engine.next_frame();
		if (frame.state.focus_changed() && frame.state.is_in_focus()) { reader.reload_out_of_date(); }
		if (frame.state.input.chord(Key::eW, Key::eLeftControl) || frame.state.input.chord(Key::eW, Key::eRightControl)) { engine.shutdown(); }

		// tick
		e1.transform.set_orientation(glm::rotate(e1.transform.orientation(), glm::radians(frame.dt.count() * 10.0f), up_v));
		free_cam.tick(engine.camera.transform, frame.state.input, frame.dt);
		ImGui::ShowDemoWindow();

		engine.render(scene, Rgba::from({0.1f, 0.1f, 0.1f, 1.0f}));
	}
}
} // namespace
} // namespace levk

int main(int argc, char** argv) {
	namespace logger = levk::logger;
	auto instance = logger::Instance{};
	try {
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
