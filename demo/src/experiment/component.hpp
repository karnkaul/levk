#pragma once
#include <levk/serializable.hpp>
#include <levk/util/id.hpp>
#include <levk/util/ptr.hpp>
#include <levk/util/time.hpp>

namespace levk::experiment {
class Entity;
class Scene;

class Component : public ISerializable {
  public:
	virtual ~Component() = default;

	virtual void tick(Time dt) = 0;

	Id<Component> id() const { return m_self; }
	Entity& entity() const;
	Scene& scene() const;

  protected:
  private:
	Id<Component> m_self{};
	Id<Entity> m_entity{};
	Ptr<Scene> m_scene{};

	friend class Entity;
};
} // namespace levk::experiment
