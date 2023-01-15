#include <imgui.h>
#include <editor.hpp>
#include <levk/util/error.hpp>

namespace levk {
namespace {
void run(std::string data_path) {
	auto editor = Editor{std::move(data_path)};

	editor.runtime.show();
	while (editor.runtime.is_running()) {
		auto frame = editor.runtime.next_frame();

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
