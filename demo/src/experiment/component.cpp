#include <experiment/component.hpp>
#include <experiment/scene.hpp>
#include <cassert>

namespace levk::experiment {
Entity& Component::entity() const {
	assert(m_entity);
	return scene().get(m_entity);
}

Scene& Component::scene() const {
	assert(m_scene);
	return *m_scene;
}
} // namespace levk::experiment
