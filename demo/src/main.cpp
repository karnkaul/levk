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

#include <editor.hpp>

namespace levk {
namespace fs = std::filesystem;

namespace {
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
				editor.import_gltf(drop.c_str(), export_path.generic_string());
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
