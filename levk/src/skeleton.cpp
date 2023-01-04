#include <levk/skeleton.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/visitor.hpp>
#include <levk/util/zip_ranges.hpp>

namespace levk {
namespace {
using JointToNode = std::unordered_map<std::size_t, Id<Node>>;
using Joint = Skeleton::Joint;

void add_to(Node::Tree& out_tree, JointToNode& out_map, std::span<Joint const> joints, Id<Joint> id, Id<Node> parent) {
	auto const& joint = joints[id];
	if (out_map.contains(joint.self)) { return; }
	auto& self = out_tree.add({.transform = joint.transform, .name = joint.name, .parent = parent});
	out_map.insert_or_assign(joint.self, self.id());
	for (auto const& child : joint.children) { add_to(out_tree, out_map, joints, child, self.id()); }
}
} // namespace

Time JointAnimation::Sampler::duration() const {
	return std::visit([](auto const& s) { return s.duration(); }, storage);
}

void JointAnimation::Sampler::update(Transform& out, Time time) const {
	auto const visitor = Visitor{
		[time, &out](JointAnimation::Translate const& translate) {
			if (auto const p = translate(time)) { out.set_position(*p); }
		},
		[time, &out](JointAnimation::Rotate const& rotate) {
			if (auto const o = rotate(time)) { out.set_orientation(*o); }
		},
		[time, &out](JointAnimation::Scale const& scale) {
			if (auto const s = scale(time)) { out.set_scale(*s); }
		},
	};
	std::visit(visitor, storage);
}

Time JointAnimation::duration() const {
	auto ret = Time{};
	for (auto const& sampler : samplers) { ret = std::max(ret, sampler.duration()); }
	return ret;
}

void SkeletalAnimation::update(Node::Locator node_locator, Time time, Source const& source) const {
	for (auto const [target_id, sampler] : zip_ranges(target_nodes, source.animation.samplers)) {
		auto& joint = node_locator.get(target_id);
		sampler.update(joint.transform, time);
	}
}

Skeleton::Instance Skeleton::instantiate(Node::Tree& out, Id<Node> root) const {
	if (!root) {
		logger::warn("[SkeletonInstance] [{}] No root node provided, creating one instead", name);
		root = out.add({.name = name}).id();
	}
	if (!out.find(root)) {
		logger::warn("[SkeletonInstance] [{}] Invalid root node provided, creating one instead", name);
		root = out.add({.name = name}).id();
	}
	auto ret = Instance{.root = root, .source = self};
	auto map = JointToNode{};
	for (std::size_t i = 0; i < joints.size(); ++i) { add_to(out, map, joints, i, root); }
	for (auto const& joint : joints) {
		assert(map.contains(joint.self));
		ret.joints.push_back(map[joint.self]);
	}
	ret.animations.reserve(clips.size());
	for (auto const& src_animation : clips) {
		auto animation = Animation{};
		for (auto const& channel : src_animation.channels) {
			auto animator = TransformAnimator{};
			animator.sampler = channel.sampler;
			assert(map.contains(channel.target));
			animator.target = map[channel.target];
			animation.add(std::move(animator));
		}
		ret.animations.push_back(std::move(animation));
	}
	ret.animations2.reserve(animation_sources.size());
	for (auto const [in_animation, index] : enumerate(animation_sources)) {
		auto& out_animation = ret.animations2.emplace_back(SkeletalAnimation{.skeleton = self, .source = index});
		for (auto const& joint : in_animation.target_joints) {
			assert(map.contains(joint));
			out_animation.target_nodes.push_back(map[joint]);
		}
	}
	return ret;
}
} // namespace levk
