#include <impl/frame_profiler.hpp>
#include <levk/runtime.hpp>
#include <levk/util/error.hpp>
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

std::string Runtime::find_directory(char const* exe_path, std::span<std::string_view const> uri_patterns) {
	if (uri_patterns.empty()) { return {}; }
	return find_dir(fs::absolute(exe_path), uri_patterns).generic_string();
}

Runtime::Runtime(NotNull<std::unique_ptr<DataSource>> data_source, Engine::CreateInfo const& create_info)
	: m_data_source(std::move(data_source)), m_context(m_data_source.get().get(), create_info) {}

void Runtime::run() {
	setup();
	m_context.show();
	while (m_context.is_running()) {
		m_context.next_frame();
		auto const dt = m_delta_time();
		FrameProfiler::instance().profile(FrameProfile::Type::eTick);
		auto* monitor = m_context.asset_providers.get().uri_monitor();
		auto const& state = m_context.engine.get().window().state();
		if (monitor && state.focus_changed() && state.is_in_focus()) {
			monitor->dispatch_modified();
			m_context.asset_providers.get().reload_out_of_date();
		}
		tick(dt);
		m_context.scene_manager.get().tick(dt);
		render();
	}
}

void Runtime::render() const { m_context.render(); }
} // namespace levk
