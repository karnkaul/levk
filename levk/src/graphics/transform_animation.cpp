#include <levk/graphics/transform_animation.hpp>
#include <levk/util/visitor.hpp>

namespace levk {
Time TransformAnimation::Sampler::duration() const {
	return std::visit([](auto const& s) { return s.duration(); }, storage);
}

void TransformAnimation::Sampler::update(Transform& out, Time time) const {
	auto const visitor = Visitor{
		[time, &out](TransformAnimation::Translate const& translate) {
			if (auto const p = translate(time)) { out.set_position(*p); }
		},
		[time, &out](TransformAnimation::Rotate const& rotate) {
			if (auto const o = rotate(time)) { out.set_orientation(*o); }
		},
		[time, &out](TransformAnimation::Scale const& scale) {
			if (auto const s = scale(time)) { out.set_scale(*s); }
		},
	};
	std::visit(visitor, storage);
}

Time TransformAnimation::duration() const {
	auto ret = Time{};
	for (auto const& sampler : samplers) { ret = std::max(ret, sampler.duration()); }
	return ret;
}
} // namespace levk
