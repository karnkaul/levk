#include <graphics/vulkan/primitive.hpp>
#include <graphics/vulkan/render_object.hpp>

namespace levk::vulkan {
std::vector<RenderObject> RenderObject::build_objects(DrawList const& draw_list, HostBuffer::Pool& buffer_pool) {
	auto joints_mats = std::vector<BufferView>{};
	joints_mats.reserve(draw_list.skins().size());
	for (auto const& skin : draw_list.skins()) {
		auto& joints_buffer = buffer_pool.next(vk::BufferUsageFlagBits::eStorageBuffer);
		auto const write_joints = [&](glm::mat4& out, std::size_t index) { out = skin.joints_global_transforms[index] * skin.inverse_bind_matrices[index]; };
		write_array<glm::mat4>(skin.joints_global_transforms.size(), joints_buffer, write_joints);
		joints_mats.push_back(joints_buffer.view());
	}

	auto ret = std::vector<RenderObject>{};
	ret.reserve(draw_list.drawables().size());
	for (auto const& drawable : draw_list.drawables()) {
		auto const* primitive = drawable.primitive.get();
		if (!primitive) { continue; }
		ret.push_back(build(drawable, *primitive, buffer_pool, joints_mats));
	}
	return ret;
}

RenderObject RenderObject::build(Drawable drawable, Primitive const& primitive, HostBuffer::Pool& buffer_pool, std::span<BufferView const> joints_mats) {
	auto ret = RenderObject{drawable};

	if (primitive.layout().instances_binding) {
		auto transform_instances = drawable.instances;
		if (transform_instances.empty()) {
			static auto const default_instance{Transform{}};
			transform_instances = {&default_instance, 1};
		}
		auto& instance_buffer = buffer_pool.next(vk::BufferUsageFlagBits::eVertexBuffer);
		auto const write_instances = [&](glm::mat4& out, std::size_t i) { out = drawable.parent * transform_instances[i].matrix(); };
		write_array<glm::mat4>(transform_instances.size(), instance_buffer, write_instances);
		ret.instances.mats_vbo = instance_buffer.view();
		ret.instances.count = static_cast<std::uint32_t>(transform_instances.size());
	}

	if (primitive.layout().joints_binding) {
		assert(primitive.layout().joints > 0);
		assert(*drawable.skin_index < joints_mats.size());
		ret.joints.mats_ssbo = joints_mats[*drawable.skin_index];
	}

	return ret;
}
} // namespace levk::vulkan
