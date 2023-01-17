#pragma once
#include <levk/imcpp/common.hpp>
#include <functional>
#include <string>

namespace levk::imcpp {
struct EditorWindow {
	using DrawTo = std::function<void(OpenWindow)>;

	std::string label{};
	glm::vec2 init_size{400.0f, 300.0f};
	DrawTo draw_to{};

	Bool show{};

	void display();
};
} // namespace levk::imcpp
