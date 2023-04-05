#pragma once
#include <glm/vec2.hpp>
#include <levk/graphics/camera.hpp>
#include <levk/graphics/common.hpp>
#include <levk/graphics/lights.hpp>
#include <levk/graphics/primitive.hpp>
#include <levk/graphics/render_list.hpp>
#include <levk/util/ptr.hpp>
#include <memory>

namespace levk {
namespace vulkan {
struct Device;
}

class AssetProviders;

struct RenderDeviceCreateInfo {
	bool validation{true};
	ColourSpace swapchain{ColourSpace::eSrgb};
	Vsync vsync{Vsync::eAdaptive};
	AntiAliasing anti_aliasing{AntiAliasing::e2x};
};

struct RenderDeviceInfo {
	std::string_view name{};
	bool validation{};
	bool portability{};
	ColourSpace swapchain{};
	VsyncFlags supported_vsync{};
	Vsync current_vsync{};
	AntiAliasingFlags supported_aa{};
	AntiAliasing current_aa{};
	float render_scale{1.0f};
	Rgba clear_colour{black_v};
};

class RenderDevice {
  public:
	using Info = RenderDeviceInfo;
	using CreateInfo = RenderDeviceCreateInfo;

	struct Frame {
		NotNull<RenderList const*> render_list;
		NotNull<AssetProviders const*> asset_providers;
		NotNull<Lights const*> lights;
		NotNull<Camera const*> camera_3d;
	};

	RenderDevice(Window const& window, CreateInfo const& create_info = {});

	Info const& info() const;
	float set_render_scale(float desired);
	std::uint64_t draw_calls_last_frame() const;
	bool set_vsync(Vsync desired);
	void set_clear(Rgba clear);

	bool render(Frame const& t);

	vulkan::Device& vulkan_device() const;

  private:
	struct Deleter {
		void operator()(vulkan::Device const*) const;
	};

	std::unique_ptr<vulkan::Device, Deleter> m_vulkan_device{};
};
} // namespace levk
