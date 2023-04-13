#pragma once
#include <levk/asset/asset_list.hpp>
#include <levk/imcpp/common.hpp>
#include <levk/io/serializable.hpp>
#include <levk/scene/inspectable.hpp>
#include <levk/util/id.hpp>
#include <levk/util/time.hpp>
#include <levk/window/window_state.hpp>

namespace levk {
class Entity;
class Scene;

class Component : public Serializable, public Inspectable {
  public:
	virtual ~Component() = default;

	virtual void setup() {}
	virtual void tick(Time dt) = 0;
	virtual void add_assets(AssetList& /*out*/, dj::Json const&) const {}

	Id<Component> id() const { return m_self; }
	Ptr<Entity> owning_entity() const;
	Scene& active_scene() const;

	WindowState const& window_state() const;
	Input const& input() const { return window_state().input; }

  protected:
  private:
	Id<Component> m_self{};
	Id<Entity> m_entity{};

	friend class Entity;
};
} // namespace levk
