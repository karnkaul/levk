#pragma once
#include <levk/render_pass.hpp>

namespace levk {
struct Renderer {
	virtual ~Renderer() = default;

	virtual void render_3d(RenderPass const& render_pass) const = 0;
	virtual void render_ui(RenderPass const& render_pass) const = 0;
};
} // namespace levk
