#include <main_menu.hpp>

namespace levk {
MainMenu::Result MainMenu::display() {
	if (auto menu = imcpp::MainMenu{}) {
		if (auto file = imcpp::Menu{menu, "File"}) {
			if (ImGui::MenuItem("Exit")) { return {.action = Action::eExit}; }
		}
		if (!windows.empty()) {
			if (auto window = imcpp::Menu{menu, "Window"}) {
				for (auto& win : windows) {
					if (win.label == "--") {
						ImGui::Separator();
						continue;
					}
					if (ImGui::MenuItem(win.label.c_str(), {}, win.show.value)) { win.show.value = !win.show; }
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Close All", {})) {
					for (auto& win : windows) { win.show.value = false; }
				}
			}
		}
	}
	return {};
}
} // namespace levk
