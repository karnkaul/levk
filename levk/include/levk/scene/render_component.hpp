#pragma once
#include <levk/graphics/draw_list.hpp>
#include <levk/scene/component.hpp>

namespace levk {
class RenderComponent : public Component {
  public:
	void tick(Time) override {}
	virtual void render(DrawList& out) const = 0;
};
} // namespace levk
