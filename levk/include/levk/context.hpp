#pragma once
#include <levk/engine.hpp>
#include <levk/resources.hpp>
#include <levk/service.hpp>

namespace levk {
struct Reader;
class Scene;

class Context {
  public:
	Context(Reader& reader, Window&& window, GraphicsDevice&& graphics_device);

	void show() { return engine.get().window().show(); }
	void hide() { return engine.get().window().hide(); }
	void shutdown() { return engine.get().window().close(); }
	bool is_running() const { return engine.get().window().is_open(); }
	Frame next_frame() { return engine.get().next_frame(); }
	void render(Scene const& scene, Rgba clear = black_v);

	Service<Engine>::Instance engine;
	Service<Resources>::Instance resources;
};

struct ContextFactory {
	WindowFactory const& window;
	GraphicsDeviceFactory const& graphics_device;

	ContextFactory(WindowFactory const& window, GraphicsDeviceFactory const& graphics_device) : window(window), graphics_device(graphics_device) {}

	Context make(Reader& reader) const { return Context{reader, window.make(), graphics_device.make(reader)}; }
};

struct DesktopContextFactoryStorage {
	DesktopWindowFactory desktop_window{};
	VulkanDeviceFactory vulkan_device{};
};

struct DesktopContextFactory : DesktopContextFactoryStorage, ContextFactory {
	DesktopContextFactory() : ContextFactory(desktop_window, vulkan_device) {}
};
} // namespace levk
