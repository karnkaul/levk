#pragma once
#include <levk/collision.hpp>
#include <levk/scene/component.hpp>

namespace levk {
class ColliderAabb : public Component {
  public:
	Aabb get_aabb() const;
	void set_aabb(Aabb const& aabb);

	virtual void on_collision([[maybe_unused]] Id<Aabb> other) {}

	void setup() override;
	void tick(Duration) override;
	std::unique_ptr<Attachment> to_attachment() const override;

  private:
	Collision::Scoped m_on_collision{};
};
} // namespace levk
