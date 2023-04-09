#include <graphics/vulkan/primitive.hpp>
#include <graphics/vulkan/render_object.hpp>

namespace levk::vulkan {
std::vector<RenderObject> RenderObject::build_objects(std::span<Drawable const> drawables, HostBuffer::Pool& buffer_pool) {
	auto ret = std::vector<RenderObject>{};
	ret.reserve(drawables.size());
	for (auto const& drawable : drawables) {
		auto const* primitive = drawable.primitive.get();
		if (!primitive) { continue; }

		auto& object = ret.emplace_back(RenderObject{drawable});

		if (primitive->layout().instances_binding) {
			auto instances = drawable.instances;
			if (instances.empty()) {
				static auto const default_instance{Transform{}};
				instances = {&default_instance, 1};
			}
			auto& instance_buffer = buffer_pool.next(vk::BufferUsageFlagBits::eVertexBuffer);
			auto const write_instances = [&](glm::mat4& out, std::size_t i) { out = drawable.parent * instances[i].matrix(); };
			write_array<glm::mat4>(instances.size(), instance_buffer, write_instances);
			object.instances.mats_vbo = instance_buffer.view();
			object.instances.count = static_cast<std::uint32_t>(instances.size());
		}

		if (primitive->layout().joints_binding) {
			assert(primitive->layout().joints > 0);
			auto& joints_buffer = buffer_pool.next(vk::BufferUsageFlagBits::eStorageBuffer);
			auto const write_joints = [&](glm::mat4& out, std::size_t index) { out = drawable.joints[index] * drawable.inverse_bind_matrices[index]; };
			write_array<glm::mat4>(drawable.joints.size(), joints_buffer, write_joints);
			object.joints.mats_ssbo = joints_buffer.view();
		}
	}
	return ret;
}
} // namespace levk::vulkan
