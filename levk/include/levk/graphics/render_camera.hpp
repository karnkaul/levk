#pragma once
#include <levk/graphics/camera.hpp>
#include <levk/util/ptr.hpp>

namespace levk {
namespace vulkan {
struct RenderCamera;
struct Device;
} // namespace vulkan

class RenderCamera {
  public:
	static constexpr std::uint32_t view_set_v{0u};

	RenderCamera(vulkan::Device& device, glm::uvec2 target_extent);

	Camera const& get_camera() const;
	void set_camera(Camera camera);

	void set_target_extent(glm::uvec2 extent);

	Ptr<vulkan::RenderCamera> vulkan_camera() const { return m_impl.get(); }

  private:
	struct Deleter {
		void operator()(vulkan::RenderCamera const* ptr) const;
	};

	std::unique_ptr<vulkan::RenderCamera, Deleter> m_impl{};
};
} // namespace levk
