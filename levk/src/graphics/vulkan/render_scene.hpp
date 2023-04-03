#pragma once
#include <graphics/vulkan/common.hpp>
#include <levk/graphics/lights.hpp>

namespace levk::vulkan {
struct RenderScene {
	struct Std430DirLight {
		alignas(16) glm::vec3 direction;
		alignas(16) glm::vec3 ambient;
		alignas(16) glm::vec3 diffuse;

		static Std430DirLight make(DirLight const& light) {
			return {
				.direction = glm::normalize(light.direction * front_v),
				.ambient = glm::vec3{0.04f},
				.diffuse = light.rgb.to_vec4(),
			};
		}
	};

	DeviceView device{};
	HostBuffer dir_lights_ssbo{};

	static RenderScene make(DeviceView const& device);

	BufferView build_dir_lights(std::span<DirLight const> in);
};
} // namespace levk::vulkan
