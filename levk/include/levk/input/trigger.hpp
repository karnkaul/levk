#pragma once
#include <levk/window/window_input.hpp>
#include <vector>

namespace levk::input {
struct Trigger {
	std::vector<Key> keys{};
	Action action{Action::ePress};

	bool operator()(WindowInput const& input) const;
};
} // namespace levk::input
