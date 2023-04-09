#pragma once
#include <levk/context.hpp>
#include <levk/io/component_factory.hpp>
#include <levk/util/logger.hpp>

namespace levk {
class Scene;

class Runtime {
  public:
	virtual ~Runtime() = default;

	void run() noexcept(false);

	Context const& context() const { return m_context; }
	Context& context() { return m_context; }
	Ptr<UriMonitor const> uri_monitor() const { return m_uri_monitor ? &*m_uri_monitor : nullptr; }
	Ptr<UriMonitor> uri_monitor() { return m_uri_monitor ? &*m_uri_monitor : nullptr; }

  protected:
	Runtime(NotNull<std::unique_ptr<DataSource>> data_source, Engine::CreateInfo const& create_info = {});

	virtual void tick(Frame const& frame) = 0;
	virtual void render() const;

	virtual void setup() {}

	logger::Instance m_logger{};
	NotNull<std::unique_ptr<DataSource>> m_data_source;
	std::optional<UriMonitor> m_uri_monitor{};
	Context m_context;
};
} // namespace levk
