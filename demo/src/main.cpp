#include <imgui.h>
#include <editor.hpp>
#include <levk/util/error.hpp>

int main([[maybe_unused]] int argc, char** argv) {
	namespace logger = levk::logger;
	auto instance = logger::Instance{};
	try {
		assert(argc > 0);
		static constexpr std::string_view const data_uris[] = {"data", "demo/data"};
		auto editor = levk::Editor{levk::find_directory(argv[0], data_uris)};
		editor.clear_colour = levk::Rgba::from({0.1f, 0.1f, 0.1f, 1.0f});
		editor.run();
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
