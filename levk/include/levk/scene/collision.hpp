#pragma once
#include <levk/aabb.hpp>
#include <levk/defines.hpp>
#include <levk/node/node_tree.hpp>
#include <levk/scene/entity.hpp>
#include <levk/util/pinned.hpp>
#include <functional>
#include <unordered_map>

namespace levk {
class ColliderAabb : public Component {
  public:
	glm::vec3 aabb_size{1.0f};
	std::function<void(ColliderAabb const&)> on_collision{};

	Aabb aabb() const;

	void setup() override;
	void tick(Duration) override {}
};

class Collision : public Pinned {
  public:
	struct Entry {
		Ptr<ColliderAabb> collider{};
		Aabb aabb{};
		glm::vec3 previous_position{};
		bool colliding{};
		bool active{};
	};
	using Map = std::unordered_map<Id<Entity>::id_type, Entry>;

	void track(Entity const& entity);

	void tick(Scene const& scene, Duration dt);

	void clear();

	Map const& entries() const { return m_entries; }

	Duration time_slice{5ms};
	bool draw_aabbs{debug_v};

  protected:
	Map m_entries{};
};
} // namespace levk
