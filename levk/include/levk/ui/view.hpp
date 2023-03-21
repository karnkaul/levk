#pragma once
#include <levk/graphics/material.hpp>
#include <levk/util/time.hpp>
#include <memory>
#include <vector>

namespace levk {
struct Input;
class DrawList;
} // namespace levk

namespace levk::ui {
using Rect = Rect2D<>;

class View {
  public:
	virtual ~View() = default;

	View& operator=(View&&) = delete;

	View() noexcept;

	Uri<Texture>& texture_uri() { return m_material.textures.uris[0]; }
	Rgba& tint() { return m_material.tint; }

	Ptr<View> super_view() const { return m_super_view; }
	UnlitMaterial const& material() const { return m_material; }
	glm::vec2 global_position() const;

	Ptr<View> add_sub_view(std::unique_ptr<View> view);
	void set_destroyed() { m_destroyed = true; }
	bool is_destroyed() const { return m_destroyed; }

	std::span<std::unique_ptr<View> const> sub_views() const { return m_sub_views; }

	virtual void tick(Input const& input, Time dt);
	virtual void render(DrawList& out) const;

	glm::vec2 position{};
	float z_index{};
	float z_rotation{};

  protected:
	void draw(Primitive::Dynamic& primitive, DrawList& out) const;

	UnlitMaterial m_material{};

  private:
	std::vector<std::unique_ptr<View>> m_sub_views{};
	Ptr<View> m_super_view{};
	bool m_destroyed{};
};
} // namespace levk::ui
