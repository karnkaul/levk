#pragma once
#include <levk/asset/asset_list.hpp>
#include <levk/imcpp/common.hpp>
#include <levk/serializable.hpp>
#include <levk/util/id.hpp>
#include <levk/util/time.hpp>

namespace levk {
class Entity;
class Scene;

class Component : public Serializable {
  public:
	virtual ~Component() = default;

	virtual void tick(Time dt) = 0;
	virtual void inspect(imcpp::OpenWindow);
	virtual void add_assets(AssetList& /*out*/, dj::Json const&) const {}

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

class RenderComponent : public Component {
  public:
	void tick(Time) override {}
	virtual void render() const = 0;
};
} // namespace levk
