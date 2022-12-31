#include <levk/animation.hpp>

namespace levk {
void Animation::add(Animator animator) {
	m_animators.push_back(std::move(animator));
	for (auto& animator : m_animators) { m_duration = std::max(animator.duration(), m_duration); }
}

void Animation::update(Node::Tree& tree, Time dt) {
	if (!enabled) { return; }
	elapsed += dt * time_scale;
	for (auto& animator : m_animators) { animator.update(tree, elapsed); }
	if (elapsed > m_duration) { elapsed = {}; }
}
} // namespace levk
