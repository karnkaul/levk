#include <levk/runtime.hpp>
#include <levk/scene.hpp>
#include <levk/serializer.hpp>
#include <levk/util/error.hpp>
#include <filesystem>

#include <levk/scene.hpp>

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

Runtime::Runtime(DataSource const& data_source, UriMonitor& uri_monitor, ContextFactory const& context_factory)
	: m_context(context_factory.make(data_source, uri_monitor, m_serializer.get())), m_scene_manager(m_context.providers.get()) {}

Runtime::ReturnCode Runtime::run() {
	try {
		setup_bindings();
		m_context.show();
		while (m_context.is_running()) {
			auto frame = m_context.next_frame();
			tick(frame);
			render();
		}
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
	return EXIT_SUCCESS;
}

void Runtime::render() { m_context.render(active_scene(), clear_colour); }

void Runtime::setup_bindings() {
	m_component_factory.get().bind<SkeletonController>();
	m_component_factory.get().bind<MeshRenderer>();

	m_serializer.get().bind<LitMaterial>();
	m_serializer.get().bind<UnlitMaterial>();
}
} // namespace levk

std::string levk::find_directory(char const* exe_path, std::span<std::string_view const> uri_patterns) {
	if (uri_patterns.empty()) { return {}; }
	return find_dir(exe_path, uri_patterns).generic_string();
}
