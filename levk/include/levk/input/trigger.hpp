#pragma once
#include <levk/window/window_input.hpp>
#include <vector>

namespace levk::input {
struct Trigger {
	std::vector<Key> keys{};
	std::vector<Action> actions{Action::ePress};

	bool operator()(WindowInput const& input) const;
};
} // namespace levk::input
