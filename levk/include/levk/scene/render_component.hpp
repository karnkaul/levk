#pragma once
#include <levk/graphics/render_list.hpp>
#include <levk/scene/component.hpp>

namespace levk {
class RenderComponent : public Component {
  public:
	void tick(Time) override {}
	virtual void render(RenderList& out) const = 0;
};
} // namespace levk
