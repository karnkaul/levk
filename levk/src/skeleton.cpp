#include <levk/skeleton.hpp>

namespace levk {
namespace {
using JointToNode = std::unordered_map<std::size_t, Id<Node>>;
using Joint = Skeleton::Joint;

void add_to(Skeleton::Instance& out_skin, Node::Tree& out_tree, JointToNode& out_map, std::span<Joint const> joints, Id<Joint> id, Id<Node> parent) {
	auto const& joint = joints[id];
	if (out_map.contains(joint.self)) { return; }
	auto& self = out_tree.add({.transform = joint.transform, .name = joint.name, .parent = parent});
	out_map.insert_or_assign(joint.self, self.id());
	for (auto const& child : joint.children) { add_to(out_skin, out_tree, out_map, joints, child, self.id()); }
}
} // namespace

Skeleton::Instance Skeleton::instantiate(Node::Tree& out, Id<Node> parent) const {
	auto ret = Instance{.inverse_bind_matrices = inverse_bind_matrices, .name = name};
	auto map = JointToNode{};
	for (std::size_t i = 0; i < joints.size(); ++i) { add_to(ret, out, map, joints, i, parent); }
	for (auto const& joint : joints) {
		assert(map.contains(joint.self));
		ret.joints.push_back(map[joint.self]);
	}
	ret.animations.reserve(anims.size());
	for (auto const& src_animation : anims) {
		auto animation = Animation{};
		for (auto animator : src_animation.animators) {
			auto it = map.find(animator.target);
			assert(it != map.end());
			animator.target = it->second;
			animation.add(std::move(animator));
		}
		ret.animations.push_back(std::move(animation));
	}
	return ret;
}
} // namespace levk
