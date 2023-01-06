#include <levk/component.hpp>
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
} // namespace levk
