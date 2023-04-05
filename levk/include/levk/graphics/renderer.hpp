#pragma once
#include <levk/graphics/render_list.hpp>

namespace levk {
struct Renderer {
	virtual ~Renderer() = default;

	virtual void render(RenderList& out) const = 0;
};
} // namespace levk
