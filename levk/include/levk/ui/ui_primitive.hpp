#pragma once
#include <levk/graphics/material.hpp>
#include <levk/graphics/primitive.hpp>
#include <levk/ui/ui_node.hpp>

namespace levk {
class RenderDevice;

class UIPrimitive : public UINode {
  public:
	UIPrimitive(RenderDevice const& render_device);

	Uri<Texture>& texture_uri() { return m_material.textures.uris[0]; }
	Rgba& tint() { return m_material.tint; }

	void set_quad(QuadCreateInfo const& create_info = {});

	UnlitMaterial const& material() const { return m_material; }

	void render(DrawList& out) const override;

  protected:
	std::unique_ptr<Primitive::Dynamic> m_primitive{};
	UnlitMaterial m_material{};
};
} // namespace levk
