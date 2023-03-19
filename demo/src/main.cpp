#include <editor.hpp>

int main([[maybe_unused]] int argc, char** argv) {
	assert(argc > 0);
	static constexpr std::string_view const data_uris[] = {"data", "demo/data"};
	try {
		auto editor = levk::Editor{levk::find_directory(argv[0], data_uris)};
		return editor.run();
	} catch (...) { return EXIT_FAILURE; }
}
