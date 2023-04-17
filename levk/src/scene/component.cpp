#include <levk/defines.hpp>
#include <levk/engine.hpp>
#include <levk/scene/component.hpp>
#include <levk/scene/scene.hpp>
#include <levk/service.hpp>
#include <levk/window/window_state.hpp>
#include <cassert>

namespace levk {
WindowState const& Component::window_state() const { return Service<Engine>::locate().window().state(); }

Ptr<Entity> Component::owning_entity() const { return m_scene ? m_scene->find_entity(m_entity) : nullptr; }

Input const& Component::input() const { return window_state().input; }

Window const& Component::window() const { return Service<Engine>::locate().window(); }

RenderDevice const& Component::render_device() const { return Service<Engine>::locate().render_device(); }
} // namespace levk
