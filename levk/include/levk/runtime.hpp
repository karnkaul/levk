#pragma once
#include <levk/component_factory.hpp>
#include <levk/context.hpp>
#include <levk/scene_manager.hpp>
#include <levk/serializer.hpp>
#include <levk/util/logger.hpp>

namespace levk {
class Scene;

class Runtime {
  public:
	using ReturnCode = int;

	virtual ~Runtime() = default;

	Runtime(NotNull<DataSource const*> data_source, NotNull<UriMonitor*> uri_monitor, ContextFactory const& context_factory);

	ReturnCode run();

	Context const& context() const { return m_context; }
	Context& context() { return m_context; }
	Scene const& active_scene() const { return m_scene_manager.get().active_scene(); }
	Scene& active_scene() { return m_scene_manager.get().active_scene(); }

  protected:
	virtual void tick(Frame const& frame) = 0;

	virtual void render();
	virtual void setup_bindings();

	logger::Instance m_logger{};
	Service<Serializer>::Instance m_serializer{};
	Service<ComponentFactory>::Instance m_component_factory{};
	Context m_context;
	Service<SceneManager>::Instance m_scene_manager;
};

std::string find_directory(char const* exe_path, std::span<std::string_view const> uri_patterns);
} // namespace levk
