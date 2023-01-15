#pragma once
#include <levk/engine.hpp>
#include <levk/resources.hpp>
#include <levk/service.hpp>
#include <levk/util/reader.hpp>

namespace levk {
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

Context make_desktop_context(Reader& reader);
std::string find_directory(char const* exe_path, std::span<std::string_view const> uri_patterns);
} // namespace levk
