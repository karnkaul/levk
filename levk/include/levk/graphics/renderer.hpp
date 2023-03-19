#pragma once
#include <levk/graphics/draw_list.hpp>

namespace levk {
struct Renderer {
	virtual ~Renderer() = default;

	virtual void render_3d(DrawList& out) const = 0;
	virtual void render_ui(DrawList& out) const = 0;
};
} // namespace levk
