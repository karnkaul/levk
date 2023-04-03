#include <graphics/vulkan/render_scene.hpp>
#include <levk/graphics/common.hpp>

namespace levk::vulkan {
RenderScene RenderScene::make(DeviceView const& device) {
	auto ret = RenderScene{.device = device};
	ret.dir_lights_ssbo = HostBuffer::make(device, vk::BufferUsageFlagBits::eStorageBuffer);
	return ret;
}

BufferView RenderScene::build_dir_lights(std::span<DirLight const> in) {
	if (!in.empty()) {
		auto dir_lights = FlexArray<Std430DirLight, max_lights_v>{};
		for (auto const& light : in) {
			dir_lights.insert(Std430DirLight::make(light));
			if (dir_lights.size() == dir_lights.capacity_v) { break; }
		}
		dir_lights_ssbo.write(dir_lights.span().data(), dir_lights.span().size_bytes());
	}
	return dir_lights_ssbo.view();
}
} // namespace levk::vulkan
