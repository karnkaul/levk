#pragma once
#include <levk/graphics/draw_list.hpp>

namespace levk {
struct RenderList {
	DrawList scene{};
	DrawList ui{};

	bool empty() const { return scene.drawables().empty() && ui.drawables().empty(); }
	std::size_t size() const { return scene.drawables().size() + ui.drawables().size(); }

	RenderList& merge(RenderList const& rhs) {
		scene.merge(rhs.scene);
		ui.merge(rhs.ui);
		return *this;
	}
};
} // namespace levk
