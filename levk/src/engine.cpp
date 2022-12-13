#include <levk/engine.hpp>

namespace levk {
Engine::Engine(Window&& window, GraphicsDevice&& device, CreateInfo const& create_info) noexcept(false)
	: m_window(std::move(window)), m_device(std::move(device)) {
	m_window.create(create_info.window_extent, create_info.window_title);
	m_device.create({m_window});

	camera.transform.set_position(create_info.camera_position);

	if (create_info.autoshow) { m_window.show(); }
}

Frame Engine::next_frame() {
	m_window.poll();
	return {.state = m_window.state(), .dt = m_dt()};
}
} // namespace levk
