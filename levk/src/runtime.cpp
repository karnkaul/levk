#include <levk/runtime.hpp>
#include <levk/util/error.hpp>

namespace levk {
Runtime::Runtime(NotNull<std::unique_ptr<DataSource>> data_source, Engine::CreateInfo const& create_info)
	: m_data_source(std::move(data_source)), m_context(m_data_source.get().get(), create_info) {}

Runtime::ReturnCode Runtime::run() {
	try {
		setup();
		m_context.show();
		while (m_context.is_running()) {
			auto frame = m_context.next_frame();
			tick(frame);
			m_context.active_scene().tick(frame.state, frame.dt);
			m_context.render();
		}
	} catch (Error const& e) {
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
} // namespace levk
