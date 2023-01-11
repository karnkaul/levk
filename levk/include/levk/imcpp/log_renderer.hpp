#pragma once
#include <levk/imcpp/common.hpp>
#include <levk/imcpp/ring_buffer.hpp>
#include <levk/util/enum_array.hpp>
#include <levk/util/logger.hpp>
#include <array>

namespace levk::imcpp {
class LogRenderer {
  public:
	LogRenderer() noexcept;

	void display(NotClosed<Window> w);

	EnumArray<logger::Level, bool> show_levels{};
	int entries{100};
	bool auto_scroll{true};
	std::array<char, 128> filter{};

	RingBuffer<logger::Entry> cache{};
};
} // namespace levk::imcpp