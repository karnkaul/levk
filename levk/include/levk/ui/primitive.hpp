#pragma once
#include <levk/graphics/material.hpp>
#include <levk/graphics/primitive.hpp>
#include <levk/ui/view.hpp>

namespace levk {
class RenderDevice;

namespace ui {
class Primitive : public View {
  public:
	Primitive(RenderDevice const& render_device);

	Uri<Texture>& texture_uri() { return m_material.textures.uris[0]; }
	Rgba& tint() { return m_material.tint; }

	void set_quad(QuadCreateInfo const& create_info = {});

	UnlitMaterial const& material() const { return m_material; }

	void render(DrawList& out) const override;

  protected:
	std::unique_ptr<levk::Primitive::Dynamic> m_primitive{};
	UnlitMaterial m_material{};
};
} // namespace ui
} // namespace levk
