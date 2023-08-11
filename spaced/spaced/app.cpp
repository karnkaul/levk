#include <levk/vfs/disk_vfs.hpp>
#include <spaced/app.hpp>
#include <spaced/game/game.hpp>
#include <spaced/layout.hpp>

namespace spaced {
namespace {
auto const engine_create_info = levk::EngineCreateInfo{
	.window_extent = layout::extent_v,
	.window_title = "Asteroids",
};
}

App::App(std::string data_path) : levk::Runtime(std::make_unique<levk::DiskVfs>(std::move(data_path)), engine_create_info) {
	context().engine.get().window().lock_aspect_ratio();
}

void App::setup() {
	context().scene_manager.get().activate(std::make_unique<Game>());

	auto unlit_material = std::make_unique<levk::UnlitMaterial>();
	context().asset_providers.get().material().add("materials/unlit", std::move(unlit_material));
}

void App::tick(levk::Duration) {
	auto const& input = context().window().state().input;
	if (input.keyboard.chord(levk::Key::eW, levk::Key::eLeftControl) || input.keyboard.chord(levk::Key::eW, levk::Key::eRightControl)) {
		context().engine.get().window().close();
	}
	//
}
} // namespace spaced
