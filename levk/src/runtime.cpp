#include <levk/impl/desktop_window.hpp>
#include <levk/impl/vulkan_device.hpp>
#include <levk/runtime.hpp>
#include <levk/scene.hpp>
#include <filesystem>

namespace levk {
namespace {
namespace fs = std::filesystem;

fs::path find_dir(fs::path exe, std::span<std::string_view const> patterns) {
	auto check = [patterns](fs::path const& prefix) {
		for (auto const pattern : patterns) {
			auto path = prefix / pattern;
			if (fs::is_directory(path)) { return path; }
		}
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
} // namespace

Runtime::Runtime(Reader& reader, Window&& window, GraphicsDevice&& graphics_device)
	: engine(std::move(window), std::move(graphics_device)), resources(reader) {}

void Runtime::render(Scene const& scene, Rgba clear) { engine.get().render(scene, scene.camera, scene.lights, clear); }
} // namespace levk

levk::Runtime levk::make_desktop_runtime(Reader& reader) { return Runtime{reader, DesktopWindow{}, VulkanDevice{reader}}; }

std::string levk::find_directory(char const* exe_path, std::span<std::string_view const> uri_patterns) {
	if (uri_patterns.empty()) { return {}; }
	return find_dir(exe_path, uri_patterns);
}
