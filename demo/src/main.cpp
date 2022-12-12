#include <imgui.h>
#include <levk/engine.hpp>
#include <levk/impl/desktop_window.hpp>
#include <levk/impl/vulkan_device.hpp>
#include <levk/util/error.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/reader.hpp>
#include <levk/util/visitor.hpp>
#include <filesystem>

namespace levk {
namespace fs = std::filesystem;

namespace {
Engine make_engine(Reader& reader) { return Engine{DesktopWindow{}, VulkanDevice{reader}}; }

struct Dummy : GraphicsRenderer {
	void render() final {}
};

struct MeshRenderer : GraphicsRenderer {
	Mesh mesh{};
	Ptr<GraphicsDevice> device{};

	MeshRenderer(GraphicsDevice& device, Geometry const& geometry) : device(&device) {
		mesh.primitives.push_back(Mesh::Primitive{.geometry = device.make_mesh_geometry(geometry)});
	}

	void render() final { device->render(mesh); }
};

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

void run(fs::path data_path) {
	auto reader = FileReader{};
	reader.mount(data_path.generic_string());
	auto engine = make_engine(reader);
	auto geometry = Geometry{};
	geometry.vertices = {
		Vertex{.position = {-0.5f, -0.5f, 0.0f}},
		Vertex{.position = {0.5f, -0.5f, 0.0f}},
		Vertex{.position = {0.0f, 0.5f, 0.0f}},
	};
	auto renderer = MeshRenderer{engine.device(), geometry};
	engine.show();
	while (engine.is_running()) {
		auto frame = engine.next_frame();
		auto const visitor = Visitor{
			[&](auto const&) {},
			[&](event::Close) { engine.shutdown(); },
			[&](event::Focus focus) {
				if (focus.gained) { reader.reload_out_of_date(); }
			},
		};
		for (auto const& event : frame.events) { std::visit(visitor, event); }

		// tick
		ImGui::ShowDemoWindow();

		engine.render(renderer, Rgba::from({0.1f, 0.1f, 0.1f, 1.0f}));
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
