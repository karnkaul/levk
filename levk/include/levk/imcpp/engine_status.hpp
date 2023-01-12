#pragma once
#include <levk/engine.hpp>
#include <levk/imcpp/common.hpp>
#include <levk/imcpp/ring_buffer.hpp>

namespace levk::imcpp {
class EngineStatus {
  public:
	void draw_to(NotClosed<Window> w, Engine& engine, Time dt);

  private:
	RingBuffer<float> m_dts{};
};
} // namespace levk::imcpp
