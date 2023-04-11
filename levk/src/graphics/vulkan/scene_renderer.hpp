#pragma once
#include <graphics/vulkan/device.hpp>
#include <graphics/vulkan/framebuffer.hpp>
#include <graphics/vulkan/render_object.hpp>
#include <levk/graphics/lights.hpp>

namespace levk {
class Scene;

namespace vulkan {
struct PipelineBuilder;

struct SceneRenderer : Device::Renderer {
	struct GlobalLayout {
		vk::UniqueDescriptorSetLayout global_set_layout{};
		vk::UniquePipelineLayout global_pipeline_layout{};
	};

	struct Frame {
		struct Std140View {
			glm::mat4 mat_vp;
			glm::vec4 vpos_exposure;
			glm::mat4 mat_shadow;
			glm::vec4 shadow_dir;
		};

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

		glm::quat primary_light_direction{glm::identity<glm::quat>()};
		glm::mat4 primary_light_mat{1.0f};
		Camera camera_3d{};
		std::vector<RenderObject> opaque{};
		std::vector<RenderObject> transparent{};
		std::vector<RenderObject> ui{};
	};

	DeviceView device{};

	Buffered<HostBuffer::Pool> buffer_pools{};
	HostBuffer view_ubo_3d{};
	HostBuffer view_ubo_ui{};
	HostBuffer dir_lights_ssbo{};
	GlobalLayout global_layout{};

	Frame frame{};

	ScopedDeviceBlocker device_block{};

	SceneRenderer(DeviceView const& device);

	void build_frame(Scene const& scene, RenderList const& render_list);

	void render_shadow(vk::CommandBuffer cb, Depthbuffer& depthbuffer, glm::vec2 map_size) final;
	void render_3d(vk::CommandBuffer cb, Framebuffer& framebuffer, ImageView const& shadow_map) final;
	void render_ui(vk::CommandBuffer cb, Framebuffer& framebuffer, ImageView const& output_3d) final;

	void bind_view_set(vk::CommandBuffer cb, HostBuffer& out_ubo, Camera const& camera, glm::uvec2 const extent);
	void draw_3d_to_ui(vk::CommandBuffer cb, ImageView const& output_3d, PipelineBuilder& pipeline_builder, vk::Extent2D extent);
};
} // namespace vulkan
} // namespace levk
