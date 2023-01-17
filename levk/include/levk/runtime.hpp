#pragma once
#include <levk/context.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/reader.hpp>
#include <filesystem>

namespace levk {
class Runtime {
  public:
	using ReturnCode = int;

	virtual ~Runtime() = default;

	Runtime(std::unique_ptr<Reader> reader, ContextFactory const& context_factory);

	ReturnCode run();

	Rgba clear_colour{};

  protected:
	virtual void tick(Frame const& frame) = 0;
	virtual void render() = 0;

	logger::Instance m_logger{};
	std::unique_ptr<Reader> m_reader{};
	Context m_context;
};

std::string find_directory(char const* exe_path, std::span<std::string_view const> uri_patterns);
} // namespace levk
