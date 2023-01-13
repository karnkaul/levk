#include <imgui.h>
#include <levk/util/visitor.hpp>
#include <main_menu.hpp>

namespace levk {
MainMenu::Result MainMenu::display_menu() {
	if (auto menu = imcpp::MainMenu{}) {
		if (auto file = imcpp::Menu{menu, "File"}) {
			if (ImGui::MenuItem("Exit")) { return {.action = Action::eExit}; }
		}
		if (!entries.empty()) {
			auto const visitor = Visitor{
				[](Separator) { ImGui::Separator(); },
				[](auto& t) {
					if (ImGui::MenuItem(t.label.c_str(), {}, t.show.value)) { t.show.value = !t.show; }
				},
			};
			if (auto window = imcpp::Menu{menu, "Window"}) {
				for (auto& entry : entries) { std::visit(visitor, entry); }
				ImGui::Separator();
				if (ImGui::MenuItem("Close All", {})) {
					auto const visitor = Visitor{
						[](Separator) {},
						[](auto& t) { t.show.value = false; },
					};
					for (auto& entry : entries) { std::visit(visitor, entry); }
				}
			}
		}
	}
	return {};
}

void MainMenu::display_windows() {
	auto const visitor = Visitor{
		[](Separator) {},
		[](imcpp::EditorWindow& ew) { ew.display(); },
		[](Custom& custom) {
			if (custom.show) { custom.draw(custom.show.value); }
		},
	};
	for (auto& entry : entries) { std::visit(visitor, entry); }
}
} // namespace levk
