#include <imgui.h>
#include <levk/imcpp/editor_window.hpp>
#include <levk/util/visitor.hpp>

namespace levk::imcpp {
void EditorWindow::display() {
	if (show) {
		ImGui::SetNextWindowSize({init_size.x, init_size.y}, ImGuiCond_Once);
		if (auto w = Window{label.c_str(), &show.value}) { draw_to(NotClosed{w}); }
	}
}
} // namespace levk::imcpp
