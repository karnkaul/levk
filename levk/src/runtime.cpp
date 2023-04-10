#include <impl/frame_profiler.hpp>
#include <levk/runtime.hpp>
#include <levk/util/error.hpp>

namespace levk {
Runtime::Runtime(NotNull<std::unique_ptr<DataSource>> data_source, Engine::CreateInfo const& create_info)
	: m_data_source(std::move(data_source)), m_context(m_data_source.get().get(), create_info) {}

void Runtime::run() {
	setup();
	m_context.show();
	while (m_context.is_running()) {
		auto frame = m_context.next_frame();
		FrameProfiler::instance().profile(FrameProfile::Type::eTick);
		auto* monitor = m_context.asset_providers.get().uri_monitor();
		if (monitor && frame.state.focus_changed() && frame.state.is_in_focus()) {
			monitor->dispatch_modified();
			m_context.asset_providers.get().reload_out_of_date();
		}
		tick(frame);
		m_context.scene_manager.get().tick(frame.state, frame.dt);
		render();
	}
}

void Runtime::render() const { m_context.render(); }
} // namespace levk
