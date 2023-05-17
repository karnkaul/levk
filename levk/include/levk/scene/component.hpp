#pragma once
#include <levk/level/attachment.hpp>
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
struct WindowInput;
class Window;
class RenderDevice;
class Collision;

class Component {
  public:
	virtual ~Component() = default;

	virtual void setup() {}
	virtual void tick(Duration dt) = 0;
	virtual std::unique_ptr<Attachment> to_attachment() const { return {}; }

	Id<Component> id() const { return m_id; }
	Ptr<Scene> owning_scene() const { return m_scene; }
	Ptr<Entity> owning_entity() const;
	Ptr<Collision> scene_collision() const;

	WindowState const& window_state() const;
	WindowInput const& window_input() const;
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
	void tick(Duration) override {}

	virtual void render(DrawList& out) const = 0;
};
} // namespace levk
