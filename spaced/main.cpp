#include <spaced/app.hpp>

int main(int argc, char** argv) {
	try {
		if (argc < 1) { throw std::runtime_error{"Zero arguments passed to main"}; }
		std::string_view const uri_patterns[] = {"editor/data"};
		auto data_path = levk::Runtime::find_directory(argv[0], uri_patterns);
		spaced::App{std::move(data_path)}.run();
	} catch (std::runtime_error const& error) {
		levk::g_logger.error("Fatal error: {}", error.what());
		return EXIT_FAILURE;
	}
}
