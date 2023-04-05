#include <graphics/vulkan/render_camera.hpp>
#include <levk/util/zip_ranges.hpp>

namespace levk::vulkan {
GlobalLayout::Storage RenderCamera::make_layout(vk::Device device) {
	auto ret = GlobalLayout::Storage{};
	static constexpr auto stages_v = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
	auto const dslb = vk::DescriptorSetLayoutBinding{0u, vk::DescriptorType::eUniformBuffer, 1u, stages_v};
	auto const dslci = vk::DescriptorSetLayoutCreateInfo{{}, dslb};
	ret.global_set_layout = device.createDescriptorSetLayoutUnique(dslci);
	auto plci = vk::PipelineLayoutCreateInfo{};
	plci.setLayoutCount = 1u;
	plci.pSetLayouts = &*ret.global_set_layout;
	ret.global_pipeline_layout = device.createPipelineLayoutUnique(plci);
	return ret;
}

RenderCamera RenderCamera::make(DeviceView device) {
	auto ret = RenderCamera{.device = device, .view_ubo = HostBuffer::make(device, vk::BufferUsageFlagBits::eUniformBuffer)};
	return ret;
}

void RenderCamera::bind_view_set(vk::CommandBuffer cb, glm::uvec2 const extent) {
	auto const view = Std140View{
		.mat_vp = camera.projection(extent) * camera.view(),
		.vpos_exposure = {camera.transform.position(), camera.exposure},
	};
	view_ubo.write(&view, sizeof(view));
	auto const buffer_view = view_ubo.view();
	auto set0 = device.set_allocator->allocate(device.global_layout.global_set_layout);
	auto const dbi = vk::DescriptorBufferInfo{buffer_view.buffer, {}, buffer_view.size};
	auto wds = vk::WriteDescriptorSet{set0, 0u, 0u, 1u, vk::DescriptorType::eUniformBuffer};
	wds.pBufferInfo = &dbi;
	device.device.updateDescriptorSets(wds, {});

	cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, device.global_layout.global_pipeline_layout, 0u, set0, {});
}
} // namespace levk::vulkan
