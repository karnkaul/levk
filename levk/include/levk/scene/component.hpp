#pragma once
#include <levk/util/id.hpp>
#include <levk/util/ptr.hpp>
#include <levk/util/time.hpp>

namespace dj {
class Json;
}

namespace levk {
class Entity;
class Scene;
class DrawList;
struct WindowState;
struct Input;
class Window;
class RenderDevice;

class Component {
  public:
	virtual ~Component() = default;

	virtual void setup() {}
	virtual void tick(Time dt) = 0;

	Id<Component> id() const { return m_id; }
	Ptr<Scene> owning_scene() const { return m_scene; }
	Ptr<Entity> owning_entity() const;

	WindowState const& window_state() const;
	Input const& input() const;
	Window const& window() const;
	RenderDevice const& render_device() const;

  protected:
  private:
	Id<Component> m_id{};
	Id<Entity> m_entity{};
	Ptr<Scene> m_scene{};

	friend class Entity;
};

class RenderComponent : public Component {
  public:
	void tick(Time) override {}

	virtual void render(DrawList& out) const = 0;
};
} // namespace levk
