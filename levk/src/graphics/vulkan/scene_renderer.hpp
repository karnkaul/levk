#pragma once
#include <graphics/vulkan/device.hpp>
#include <graphics/vulkan/framebuffer.hpp>
#include <graphics/vulkan/primitive.hpp>
#include <graphics/vulkan/render_object.hpp>
#include <levk/graphics/lights.hpp>
#include <levk/graphics/material.hpp>
#include <levk/util/enum_array.hpp>
#include <optional>

namespace levk {
class Scene;
class Collision;

namespace vulkan {
struct PipelineBuilder;

class CollisionRenderer {
  public:
	CollisionRenderer(DeviceView device);

	void update(Collision const& collision);
	void render(DrawList& out) const;

  private:
	struct Entry {
		mutable vulkan::HostPrimitive primitive;
		Ptr<levk::Material const> material{};
	};

	struct Pool {
		DeviceView device{};
		std::vector<Entry> entries{};
		std::size_t current_index{};

		Entry& next();
	};

	Pool m_pool;
	UnlitMaterial m_red{};
	UnlitMaterial m_green{};
};

struct SceneRenderer : Device::Renderer {
	enum class Xbo { eSkybox, e3d, eUi, eDirLights, eCOUNT_ };

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
		std::optional<RenderObject> skybox{};
		std::vector<RenderObject> opaque{};
		std::vector<RenderObject> transparent{};
		std::vector<RenderObject> ui{};
		std::vector<RenderObject> overlay{};
	};

	DeviceView device{};

	Buffered<HostBuffer::Pool> buffer_pools{};
	EnumArray<Xbo, HostBuffer> xbos{};
	UploadedPrimitive skybox_cube;
	SkyboxMaterial skybox_material{};
	GlobalLayout global_layout{};

	CollisionRenderer collision_renderer;
	Frame frame{};
	Ptr<Scene const> scene{};
	Ptr<RenderList const> render_list{};

	ScopedDeviceBlocker device_block{};

	SceneRenderer(DeviceView const& device);

	void update(Scene const& scene);

	void next_frame() final;
	void render_shadow(vk::CommandBuffer cb, Depthbuffer& depthbuffer) final;
	void render_3d(vk::CommandBuffer cb, Framebuffer& framebuffer, ImageView const& shadow_map) final;
	void render_ui(vk::CommandBuffer cb, Framebuffer& framebuffer, ImageView const& output_3d) final;

	void bind_view_set(vk::CommandBuffer cb, HostBuffer& out_ubo, Camera const& camera, glm::uvec2 const extent);
	void draw_3d_to_ui(vk::CommandBuffer cb, ImageView const& output_3d, PipelineBuilder& pipeline_builder, vk::Extent2D extent);
};
} // namespace vulkan
} // namespace levk
