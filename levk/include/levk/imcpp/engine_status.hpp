#pragma once
#include <levk/engine.hpp>
#include <levk/imcpp/common.hpp>
#include <vector>

namespace levk::imcpp {
class EngineStatus {
  public:
	void draw_to(NotClosed<Window> w, Engine& engine, Time dt);

  private:
	std::vector<float> m_dts{};
	std::size_t m_offset{};
	int m_capacity{100};
};
} // namespace levk::imcpp
