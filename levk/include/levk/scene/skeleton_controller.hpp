#pragma once
#include <levk/graphics/skeleton.hpp>
#include <levk/scene/component.hpp>

namespace levk {
class SkeletonController : public Component {
  public:
	using Animation = SkeletalAnimation;

	std::optional<Id<Animation>> enabled{};
	float time_scale{1.0f};
	Time elapsed{};

	void change_animation(std::optional<Id<Animation>> index);
	glm::mat4 global_transform(Id<Node> node_id) const;

	void tick(Time dt) override;
};
} // namespace levk
