#include <levk/context.hpp>
#include <levk/scene.hpp>

namespace levk {
Context::Context(Reader& reader, Window&& window, GraphicsDevice&& graphics_device)
	: engine(std::move(window), std::move(graphics_device)), resources(reader) {}

void Context::render(Scene const& scene, Rgba clear) { engine.get().render(scene, scene.camera, scene.lights, clear); }
} // namespace levk
