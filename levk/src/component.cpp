#include <imgui.h>
#include <levk/component.hpp>
#include <levk/defines.hpp>
#include <levk/scene.hpp>
#include <cassert>

namespace levk {
Entity& Component::entity() const {
	assert(m_entity);
	return scene().get(m_entity);
}

Scene& Component::scene() const {
	assert(m_scene);
	return *m_scene;
}

void Component::inspect(imcpp::OpenWindow) {
	if constexpr (debug_v) { ImGui::Text("[Not customized]"); }
}
} // namespace levk
