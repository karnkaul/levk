#pragma once
#include <levk/component_factory.hpp>
#include <levk/context.hpp>
#include <levk/serializer.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/reader.hpp>

namespace levk {
class Scene;

class Runtime {
  public:
	using ReturnCode = int;

	virtual ~Runtime() = default;

	Runtime(std::unique_ptr<Reader> reader, ContextFactory const& context_factory);

	ReturnCode run();

	bool load_into(Scene& out, Uri<Scene> const& uri, Bool reload_asset = {}) const;
	bool save(Scene const& scene, Uri<Scene> const& uri) const;

	Rgba clear_colour{};

  protected:
	virtual void tick(Frame const& frame) = 0;
	virtual void render() = 0;

	virtual void setup_bindings();

	logger::Instance m_logger{};
	Service<Serializer>::Instance m_serializer{};
	Service<ComponentFactory>::Instance m_component_factory{};
	std::unique_ptr<Reader> m_reader{};
	Context m_context;
};

std::string find_directory(char const* exe_path, std::span<std::string_view const> uri_patterns);
} // namespace levk
