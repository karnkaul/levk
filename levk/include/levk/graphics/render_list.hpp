#pragma once
#include <levk/graphics/draw_list.hpp>

namespace levk {
struct RenderList {
	DrawList opaque{};
	DrawList transparent{};
	DrawList ui{};

	bool empty() const { return opaque.drawables().empty() && transparent.drawables().empty() && ui.drawables().empty(); }
	std::size_t size() const { return opaque.drawables().size() + transparent.drawables().size() + ui.drawables().size(); }

	void merge(RenderList const& rhs) {
		opaque.merge(rhs.opaque);
		transparent.merge(rhs.transparent);
		ui.merge(rhs.ui);
	}
};
} // namespace levk
