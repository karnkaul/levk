#pragma once
#include <levk/animator.hpp>
#include <vector>

namespace levk {
class Animation {
  public:
	void add(Animator animator);
	void update(Node::Locator node_locator, Time dt);

	Time duration() const { return m_duration; }
	std::span<Animator const> animators() const { return m_animators; }

	std::string name{};
	float time_scale{1.0f};
	Time elapsed{};
	bool enabled{true};

  private:
	std::vector<Animator> m_animators{};
	Time m_duration{};
};
} // namespace levk
