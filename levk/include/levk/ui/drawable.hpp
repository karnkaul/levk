#pragma once
#include <levk/ui/view.hpp>

namespace levk {
class RenderDevice;
}

namespace levk::ui {
struct QuadCreateInfo {
	glm::vec2 size{100.0f, 100.0f};
	glm::vec3 rgb{1.0f};
	glm::vec3 origin{};
	UvRect uv{uv_rect_v};
};

class Drawable : public View {
  public:
	Drawable(RenderDevice const& render_device);

	void render(DrawList& out) const override;

	void set_quad(QuadCreateInfo const& create_info = {});

	std::unique_ptr<Primitive::Dynamic> primitive{};
};
} // namespace levk::ui
