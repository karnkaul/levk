#pragma once
#include <levk/asset/asset_providers.hpp>
#include <levk/engine.hpp>
#include <levk/service.hpp>

namespace levk {
struct Reader;
class Scene;

class Context {
  public:
	Context(DataSource const& data_source, UriMonitor& uri_monitor, Serializer const& serializer, Window&& window, GraphicsDevice&& graphics_device);

	void show() { return engine.get().window().show(); }
	void hide() { return engine.get().window().hide(); }
	void shutdown() { return engine.get().window().close(); }
	bool is_running() const { return engine.get().window().is_open(); }
	Frame next_frame() { return engine.get().next_frame(); }
	void render(Scene const& scene, Rgba clear = black_v);

	DataSource const& data_source() const { return providers.get().data_source(); }

	Service<Engine>::Instance engine;
	Service<AssetProviders>::Instance providers;
};

struct ContextFactory {
	WindowFactory const& window;
	GraphicsDeviceFactory const& graphics_device;

	ContextFactory(WindowFactory const& window, GraphicsDeviceFactory const& graphics_device) : window(window), graphics_device(graphics_device) {}

	Context make(DataSource const& data_source, UriMonitor& uri_monitor, Serializer const& serializer) const {
		return Context{data_source, uri_monitor, serializer, window.make(), graphics_device.make()};
	}
};

struct DesktopContextFactoryStorage {
	DesktopWindowFactory desktop_window{};
	VulkanDeviceFactory vulkan_device{};
};

struct DesktopContextFactory : DesktopContextFactoryStorage, ContextFactory {
	DesktopContextFactory() : ContextFactory(desktop_window, vulkan_device) {}
};
} // namespace levk
