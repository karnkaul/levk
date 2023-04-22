#pragma once
#include <levk/asset/asset_providers.hpp>
#include <levk/engine.hpp>
#include <levk/io/serializer.hpp>
#include <levk/scene/scene_manager.hpp>
#include <levk/service.hpp>

namespace levk {
class Scene;

class Context {
  public:
	Context(NotNull<DataSource const*> data_source, Engine::CreateInfo const& create_info = {});

	void show() { return engine.get().window().show(); }
	void hide() { return engine.get().window().hide(); }
	void shutdown() { return engine.get().window().close(); }
	bool is_running() const { return engine.get().window().is_open(); }
	void next_frame() { return engine.get().next_frame(); }
	void render() const;

	DataSource const& data_source() const { return asset_providers.get().data_source(); }
	RenderDevice& render_device() const { return engine.get().render_device(); }
	Window& window() const { return engine.get().window(); }
	Scene& active_scene() { return scene_manager.get().active_scene(); }
	Scene const& active_scene() const { return scene_manager.get().active_scene(); }

	Service<Serializer>::Instance serializer{};
	Service<Engine>::Instance engine;
	Service<AssetProviders>::Instance asset_providers;
	Service<SceneManager>::Instance scene_manager;
};
} // namespace levk
