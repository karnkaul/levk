#include <levk/graphics/skeleton.hpp>
#include <levk/util/zip_ranges.hpp>

namespace levk {
void SkeletalAnimation::update_nodes(NodeLocator node_locator, Time time) const {
	assert(target_joints.size() == animation.samplers.size());
	for (auto const [target_id, sampler] : zip_ranges(target_joints, animation.samplers)) {
		auto* joint = node_locator.find(target_id);
		if (!joint) { continue; }
		sampler.update(joint->transform, time);
	}
}
} // namespace levk
