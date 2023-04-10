#pragma once
#include <levk/imcpp/common.hpp>
#include <levk/imcpp/input_text.hpp>
#include <levk/util/enum_array.hpp>
#include <levk/util/logger.hpp>
#include <deque>

namespace levk::imcpp {
class LogDisplay : public Logger::Sink {
  public:
	LogDisplay();

	void draw_to(NotClosed<Window> w);

	void init(std::span<Logger::Entry const> entries) final;
	void on_log(Logger::Entry const& entry) final;

	static constexpr std::size_t max_entries_v{1024};

	EnumArray<Logger::Level, bool> show_levels{};
	int display_count{100};
	bool auto_scroll{true};
	InputText<> filter{};

	std::deque<Logger::Entry> entries_stack{};
};
} // namespace levk::imcpp
