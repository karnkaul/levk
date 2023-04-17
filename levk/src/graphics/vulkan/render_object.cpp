#include <graphics/vulkan/primitive.hpp>
#include <graphics/vulkan/render_object.hpp>

namespace levk::vulkan {
std::vector<RenderObject> RenderObject::build_objects(DrawList const& draw_list, HostBuffer::Pool& buffer_pool) {
	auto ret = std::vector<RenderObject>{};
	auto joints_mats = std::vector<BufferView>{};
	joints_mats.reserve(draw_list.skins().size());
	for (auto const& skin : draw_list.skins()) {
		auto& joints_buffer = buffer_pool.next(vk::BufferUsageFlagBits::eStorageBuffer);
		auto const write_joints = [&](glm::mat4& out, std::size_t index) { out = skin.joints_global_transforms[index] * skin.inverse_bind_matrices[index]; };
		write_array<glm::mat4>(skin.joints_global_transforms.size(), joints_buffer, write_joints);
		joints_mats.push_back(joints_buffer.view());
	}
	ret.reserve(draw_list.drawables().size());
	for (auto const& drawable : draw_list.drawables()) {
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
			assert(*drawable.skin_index < joints_mats.size());
			object.joints.mats_ssbo = joints_mats[*drawable.skin_index];
		}
	}
	return ret;
}

auto RenderObject::List::build(DrawList const& in, HostBuffer::Pool& buffer_pool) -> List {
	auto ret = List{};
	ret.joints_mats.reserve(in.skins().size());
	for (auto const& skin : in.skins()) {
		auto& joints_buffer = buffer_pool.next(vk::BufferUsageFlagBits::eStorageBuffer);
		auto const write_joints = [&](glm::mat4& out, std::size_t index) { out = skin.joints_global_transforms[index] * skin.inverse_bind_matrices[index]; };
		write_array<glm::mat4>(skin.joints_global_transforms.size(), joints_buffer, write_joints);
		ret.joints_mats.push_back(joints_buffer.view());
	}
	ret.objects.reserve(in.drawables().size());
	for (auto const& drawable : in.drawables()) {
		auto const* primitive = drawable.primitive.get();
		if (!primitive) { continue; }

		auto& object = ret.objects.emplace_back(RenderObject{drawable});

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
			assert(primitive->layout().joints > 0 && drawable.skin_index.has_value());
			// auto& joints_buffer = buffer_pool.next(vk::BufferUsageFlagBits::eStorageBuffer);
			// auto const write_joints = [&](glm::mat4& out, std::size_t index) { out = drawable.joints[index] * drawable.inverse_bind_matrices[index]; };
			// write_array<glm::mat4>(drawable.joints.size(), joints_buffer, write_joints);
			// object.joints.mats_ssbo = joints_buffer.view();
			assert(*drawable.skin_index < ret.joints_mats.size());
			object.joints.mats_ssbo = ret.joints_mats[*drawable.skin_index];
		}
	}
	return ret;
}
} // namespace levk::vulkan
