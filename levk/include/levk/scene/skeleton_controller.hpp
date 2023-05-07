#pragma once
#include <levk/graphics/skeleton.hpp>
#include <levk/scene/component.hpp>

namespace levk {
class SkeletonController : public Component {
  public:
	using Animation = SkeletalAnimation;

	std::optional<Id<Animation>> enabled{};
	float time_scale{1.0f};
	Duration elapsed{};

	void change_animation(std::optional<Id<Animation>> index);
	glm::mat4 global_transform(Id<Node> node_id) const;

	void tick(Duration dt) override;
	std::unique_ptr<Attachment> to_attachment() const override;
};
} // namespace levk
