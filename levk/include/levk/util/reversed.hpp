#pragma once
#include <iterator>

namespace levk {
template <typename It>
struct Reversed {
	using value_type = decltype(*std::declval<It>());
	using iterator = std::reverse_iterator<It>;
	using const_iterator = std::reverse_iterator<It>;
	using difference_type = void;

	It first{};
	It last{};

	constexpr iterator begin() const { return std::reverse_iterator<It>{last}; }
	constexpr iterator end() const { return std::reverse_iterator<It>{first}; }
};

template <typename Container>
constexpr auto reversed(Container&& container) {
	using It = decltype(std::begin(container));
	return Reversed<It>{std::begin(container), std::end(container)};
}
} // namespace levk
