#include <levk/defines.hpp>
#include <levk/engine.hpp>
#include <levk/scene/component.hpp>
#include <levk/scene/scene_manager.hpp>
#include <levk/service.hpp>
#include <cassert>

namespace levk {
Scene& Component::active_scene() const { return Service<SceneManager>::locate().active_scene(); }

WindowState const& Component::window_state() const { return Service<Engine>::locate().window().state(); }

Ptr<Entity> Component::owning_entity() const { return active_scene().find(m_entity); }
} // namespace levk
