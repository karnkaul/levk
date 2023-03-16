#pragma once
#include <levk/drawer.hpp>

namespace levk {
struct Renderer {
	virtual ~Renderer() = default;

	virtual void render_3d(Drawer const& drawer) const = 0;
	virtual void render_ui(Drawer const& drawer) const = 0;
};
} // namespace levk
