#include <imgui.h>
#include <levk/defines.hpp>
#include <levk/scene/component.hpp>
#include <levk/scene/scene_manager.hpp>
#include <levk/service.hpp>
#include <cassert>

namespace levk {
Scene& Component::active_scene() const { return Service<SceneManager>::locate().active_scene(); }

Ptr<Entity> Component::owning_entity() const { return active_scene().find(m_entity); }

void Component::inspect(imcpp::OpenWindow) {
	if constexpr (debug_v) { ImGui::Text("[Not customized]"); }
}
} // namespace levk
