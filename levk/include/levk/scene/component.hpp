#pragma once
#include <levk/asset/asset_list.hpp>
#include <levk/imcpp/common.hpp>
#include <levk/io/serializable.hpp>
#include <levk/util/id.hpp>
#include <levk/util/time.hpp>

namespace levk {
class Entity;
class Scene;
struct RenderList;

class Component : public Serializable {
  public:
	virtual ~Component() = default;

	virtual void tick(Time dt) = 0;
	virtual void inspect(imcpp::OpenWindow);
	virtual void add_assets(AssetList& /*out*/, dj::Json const&) const {}

	Id<Component> id() const { return m_self; }
	Ptr<Entity> owning_entity() const;
	Scene& active_scene() const;

  protected:
  private:
	Id<Component> m_self{};
	Id<Entity> m_entity{};

	friend class Entity;
};

class RenderComponent : public Component {
  public:
	void tick(Time) override {}
	virtual void render(RenderList& out) const = 0;
};
} // namespace levk
