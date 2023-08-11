#pragma once
#include <levk/runtime.hpp>
#include <levk/service.hpp>

namespace spaced {
class App : public levk::Runtime {
  public:
	App(std::string data_path);

  private:
	void setup() final;
	void tick(levk::Duration dt) final;
};
} // namespace spaced
