#pragma once
#include <levk/imcpp/common.hpp>
#include <levk/imcpp/input_text.hpp>
#include <levk/imcpp/ring_buffer.hpp>
#include <levk/util/enum_array.hpp>
#include <levk/util/logger.hpp>
#include <array>

namespace levk::imcpp {
class LogRenderer {
  public:
	LogRenderer() noexcept;

	void draw_to(NotClosed<Window> w);

	EnumArray<logger::Level, bool> show_levels{};
	int entries{100};
	bool auto_scroll{true};
	InputText<> filter{};

	RingBuffer<logger::Entry> cache{};
};
} // namespace levk::imcpp
