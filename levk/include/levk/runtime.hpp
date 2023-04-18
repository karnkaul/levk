#pragma once
#include <levk/context.hpp>
#include <levk/util/logger.hpp>

namespace levk {
class Scene;

class Runtime {
  public:
	virtual ~Runtime() = default;

	static std::string find_directory(char const* exe_path, std::span<std::string_view const> uri_patterns);

	void run() noexcept(false);

	Context const& context() const { return m_context; }
	Context& context() { return m_context; }

  protected:
	Runtime(NotNull<std::unique_ptr<DataSource>> data_source, Engine::CreateInfo const& create_info = {});

	virtual void tick(Time dt) = 0;
	virtual void render() const;

	virtual void setup() {}

	Logger::Instance m_logger{};
	NotNull<std::unique_ptr<DataSource>> m_data_source;
	Context m_context;
	DeltaTime m_delta_time{};
};
} // namespace levk
