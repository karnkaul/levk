#pragma once
#include <levk/imcpp/common.hpp>
#include <array>
#include <cstring>
#include <string_view>

namespace levk::imcpp {
template <std::size_t Capacity = 128>
struct InputText {
	std::array<char, Capacity> buffer{};
	std::size_t used{};

	bool operator()(char const* label, int flags = {}) {
		auto ret = input_text(label, buffer.data(), buffer.size(), flags);
		used = std::string_view{buffer.data()}.size();
		return ret;
	}

	void set(std::string_view text) {
		if (text.empty()) {
			*this = {};
			return;
		}
		used = std::min(text.size(), buffer.size());
		std::memcpy(buffer.data(), text.data(), used);
	}

	constexpr bool empty() const { return used == 0; }
	constexpr std::string_view view() const { return {buffer.data(), used}; }
	constexpr operator std::string_view() const { return view(); }
};
} // namespace levk::imcpp
