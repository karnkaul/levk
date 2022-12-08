#include <imgui.h>
#include <levk/engine.hpp>
#include <levk/impl/desktop_window.hpp>
#include <levk/impl/vulkan_device.hpp>
#include <levk/util/error.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/visitor.hpp>

namespace levk {
namespace {
Engine make_engine() { return Engine{DesktopWindow{}, VulkanDevice{}}; }

struct Dummy : GraphicsRenderer {
	void render(RenderContext&) final {}
};

void run() {
	auto engine = make_engine();
	auto renderer = Dummy{};
	engine.show();
	while (engine.is_running()) {
		auto frame = engine.next_frame();
		auto const visitor = Visitor{
			[&](auto const&) {},
			[&](event::Close) { engine.shutdown(); },
		};
		for (auto const& event : frame.events) { std::visit(visitor, event); }

		// tick
		ImGui::ShowDemoWindow();

		engine.render(renderer);
	}
}
} // namespace
} // namespace levk

int main() {
	namespace logger = levk::logger;
	auto instance = logger::Instance{};
	try {
		levk::run();
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
