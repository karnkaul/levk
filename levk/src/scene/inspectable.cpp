#include <imgui.h>
#include <levk/defines.hpp>
#include <levk/scene/inspectable.hpp>

namespace levk {
void Inspectable::inspect(imcpp::OpenWindow) {
	if constexpr (debug_v) { ImGui::Text("[Not customized]"); }
}
} // namespace levk
