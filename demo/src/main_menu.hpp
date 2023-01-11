#pragma once
#include <imgui.h>
#include <levk/imcpp/common.hpp>
#include <levk/util/bool.hpp>
#include <string>
#include <vector>

namespace levk {
struct MainMenu {
	struct Win {
		std::string label{};
		glm::vec2 init_size{400.0f, 300.0f};

		Bool show{};

		template <typename Func>
		bool display(Func&& func) {
			if (show) {
				ImGui::SetNextWindowSize({init_size.x, init_size.y}, ImGuiCond_Once);
				if (auto w = imcpp::Window{label.c_str(), &show.value}) {
					func(imcpp::NotClosed{w});
					return true;
				}
			}
			return false;
		}
	};

	enum class Action { eNone, eExit };

	struct Result {
		Action action{};
	};

	std::vector<Win> windows{};

	Result display();
};
} // namespace levk
