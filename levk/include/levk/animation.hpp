#pragma once
#include <levk/animator.hpp>
#include <vector>

namespace levk {
class Animation {
  public:
	void add(Animator animator);
	void update(Node::Tree& tree, float dt);

	float duration() const { return m_duration; }
	std::span<Animator const> animators() const { return m_animators; }

	std::string name{};
	float time_scale{1.0f};
	float elapsed{};
	bool enabled{true};

  private:
	std::vector<Animator> m_animators{};
	float m_duration{};
};
} // namespace levk
