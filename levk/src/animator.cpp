#include <levk/animator.hpp>
#include <levk/util/visitor.hpp>

namespace levk {
float TransformAnimator::duration() const {
	return std::visit([](auto const& ch) { return ch.duration(); }, channel);
}

void TransformAnimator::update(Node& node, float time) const {
	auto const visitor = Visitor{
		[time, &node](Translate const& translate) {
			if (auto const p = translate(time)) { node.transform.set_position(*p); }
		},
		[time, &node](Rotate const& rotate) {
			if (auto const o = rotate(time)) { node.transform.set_orientation(*o); }
		},
		[time, &node](Scale const& scale) {
			if (auto const s = scale(time)) { node.transform.set_scale(*s); }
		},
	};
	std::visit(visitor, channel);
}
} // namespace levk
