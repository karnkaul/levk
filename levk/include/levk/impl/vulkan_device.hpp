#pragma once
#include <vk_mem_alloc.h>
#include <glm/vec2.hpp>
#include <levk/graphics_common.hpp>
#include <levk/impl/defer_queue.hpp>
#include <levk/surface.hpp>
#include <levk/util/flex_array.hpp>
#include <levk/util/ptr.hpp>
#include <levk/util/unique.hpp>
#include <vulkan/vulkan.hpp>
#include <optional>
#include <unordered_set>

namespace levk {
template <typename T, typename U = T>
using Pair = std::pair<T, U>;

constexpr vk::Format srgb_formats_v[] = {vk::Format::eR8G8B8A8Srgb, vk::Format::eB8G8R8A8Srgb, vk::Format::eA8B8G8R8SrgbPack32};
constexpr vk::Format linear_formats_v[] = {vk::Format::eR8G8B8A8Unorm, vk::Format::eB8G8R8A8Unorm};

constexpr bool is_linear(vk::Format format) {
	return std::find(std::begin(linear_formats_v), std::end(linear_formats_v), format) != std::end(linear_formats_v);
}

constexpr bool is_srgb(vk::Format format) { return std::find(std::begin(srgb_formats_v), std::end(srgb_formats_v), format) != std::end(srgb_formats_v); }

constexpr ColourSpace colour_space(vk::Format format) { return is_srgb(format) ? ColourSpace::eSrgb : ColourSpace::eLinear; }

inline constexpr std::size_t buffering_v{2};

template <typename Type, std::size_t Size = buffering_v>
struct Rotator {
	Type t[Size]{};
	std::size_t index{};

	constexpr Type& get() { return t[index]; }
	constexpr Type const& get() const { return t[index]; }

	constexpr void rotate() { index = (index + 1) % Size; }
};

struct BufferView {
	vk::Buffer buffer{};
	vk::DeviceSize size{};
	vk::DeviceSize offset{};
	std::uint32_t count{1};
};

struct DescriptorBuffer {
	vk::Buffer buffer{};
	vk::DeviceSize size{};
	vk::DescriptorType type{};
};

struct ImageView {
	vk::Image image{};
	vk::ImageView view{};
	vk::Extent2D extent{};
};

struct DescriptorImage {
	vk::ImageView image{};
	vk::Sampler sampler{};
};

struct ImageCreateInfo {
	static constexpr auto usage_v = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;

	vk::Format format{vk::Format::eR8G8B8A8Srgb};
	vk::ImageUsageFlags usage{usage_v};
	vk::ImageAspectFlagBits aspect{vk::ImageAspectFlagBits::eColor};
	vk::ImageTiling tiling{vk::ImageTiling::eOptimal};
	std::uint32_t mip_levels{1};
	std::uint32_t array_layers{1};
	vk::SampleCountFlagBits samples{vk::SampleCountFlagBits::e1};
};

struct ImageBarrier {
	vk::ImageMemoryBarrier barrier{};
	Pair<vk::PipelineStageFlags> stages{};

	void transition(vk::CommandBuffer cb) const { cb.pipelineBarrier(stages.first, stages.second, {}, {}, {}, barrier); }
};

struct Vma {
	struct Deleter;
	struct Allocation;
	struct Buffer;
	struct Image;

	struct Mls {
		std::uint32_t mip_levels{1};
		std::uint32_t array_layers{1};
		vk::SampleCountFlagBits samples{vk::SampleCountFlagBits::e1};
	};

	static constexpr auto isr_v = vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

	vk::Device device{};
	VmaAllocator allocator{};

	Unique<Buffer, Deleter> make_buffer(vk::BufferUsageFlags usage, vk::DeviceSize size, bool host_visible) const;
	Unique<Image, Deleter> make_image(ImageCreateInfo const& info, vk::Extent2D extent, vk::ImageViewType type = vk::ImageViewType::e2D) const;
	vk::UniqueImageView make_image_view(vk::Image const image, vk::Format const format, vk::ImageSubresourceRange isr = isr_v,
										vk::ImageViewType type = vk::ImageViewType::e2D) const;
};

struct Vma::Allocation {
	Vma vma{};
	VmaAllocation allocation{};
};

struct Vma::Deleter {
	void operator()(Vma const& vma) const;
	void operator()(Buffer const& buffer) const;
	void operator()(Image const& image) const;
};

using UniqueVma = Unique<Vma, Vma::Deleter>;

struct Vma::Buffer {
	Allocation allocation{};
	vk::Buffer buffer{};
	vk::DeviceSize size{};
	void* ptr{};
};

struct Vma::Image {
	Allocation allocation{};
	vk::Image image{};
	vk::UniqueImageView view{};
	vk::Extent2D extent{};

	ImageView image_view() const { return {image, *view, extent}; }
};

using UniqueBuffer = Unique<Vma::Buffer, Vma::Deleter>;
using UniqueImage = Unique<Vma::Image, Vma::Deleter>;
struct Gpu {
	vk::PhysicalDevice device{};
	vk::PhysicalDeviceProperties properties{};
	std::uint32_t queue_family{};

	explicit operator bool() const { return !!device; }
};

struct Swapchain {
	static constexpr std::size_t max_images_v{8};

	struct Formats {
		std::vector<vk::Format> srgb{};
		std::vector<vk::Format> linear{};
	};

	struct Storage {
		FlexArray<ImageView, max_images_v> images{};
		FlexArray<vk::UniqueImageView, max_images_v> views{};
		vk::UniqueSwapchainKHR swapchain{};
		glm::uvec2 extent{};
	};

	Formats formats{};
	std::unordered_set<vk::PresentModeKHR> modes{};
	vk::SwapchainCreateInfoKHR info{};

	Storage storage{};
};

struct VulkanDevice;

class Cmd {
  public:
	struct Allocator {
		static constexpr vk::CommandPoolCreateFlags flags_v = vk::CommandPoolCreateFlagBits::eTransient;

		static Allocator make(vk::Device device, std::uint32_t queue_family, vk::CommandPoolCreateFlags flags = flags_v);

		vk::Device device{};
		vk::UniqueCommandPool pool{};

		vk::Result allocate(std::span<vk::CommandBuffer> out, bool secondary = false) const;
		vk::CommandBuffer allocate(bool secondary) const;
	};

	Cmd(VulkanDevice const& device, vk::PipelineStageFlags wait = vk::PipelineStageFlagBits::eAllCommands);
	~Cmd();

	Cmd& operator=(Cmd&&) = delete;

	vk::CommandBuffer cb{};

  private:
	VulkanDevice const& m_device;
	Allocator m_allocator{};
	vk::PipelineStageFlags m_wait{};
	vk::UniqueFence m_fence{};
};

using RenderAttachments = FlexArray<vk::ImageView, 3>;

struct RenderTarget {
	ImageView colour{};
	ImageView depth{};
	ImageView resolve{};
	vk::Extent2D extent{};

	RenderAttachments attachments() const {
		auto ret = RenderAttachments{};
		if (colour.view) { ret.insert(colour.view); }
		if (depth.view) { ret.insert(depth.view); }
		if (resolve.view) { ret.insert(resolve.view); }
		return ret;
	}

	void to_draw(vk::CommandBuffer cb);
	void to_present(vk::CommandBuffer cb);
};

struct Framebuffer {
	RenderAttachments attachments{};
	vk::Framebuffer framebuffer{};
	vk::CommandBuffer primary{};
	vk::CommandBuffer secondary{};
};

struct RenderFrame {
	struct Sync {
		vk::UniqueSemaphore draw{};
		vk::UniqueSemaphore present{};
		vk::UniqueFence drawn{};
	};

	Sync sync{};
	Cmd::Allocator cmd_alloc{};
	vk::UniqueFramebuffer framebuffer{};
	vk::CommandBuffer primary{};
	vk::CommandBuffer secondary{};

	static RenderFrame make(VulkanDevice const& device);
};

template <std::size_t Size = buffering_v>
using RenderFrames = Rotator<RenderFrame, Size>;

struct RenderPass {
	struct Target {
		UniqueImage image{};
		vk::Format format{};
	};

	vk::UniqueRenderPass render_pass{};

	Target colour{};
	Target depth{};
	vk::Extent2D extent{};
};

struct DearImGui {
	enum class State { eNewFrame, eEndFrame };

	vk::UniqueDescriptorPool pool{};
	State state{};

	void new_frame();
	void end_frame();
	void render(vk::CommandBuffer cb);
};

struct VulkanRenderContext : RenderContext {
	// Ptr<Pipes> pipes{};
	vk::CommandBuffer cb{};
};

struct VulkanDevice {
	struct Shared {
		std::mutex mutex{};
		DeferQueue defer{};
	};

	vk::UniqueInstance instance{};
	vk::UniqueDebugUtilsMessengerEXT debug{};
	Gpu gpu{};
	vk::UniqueDevice device{};
	vk::UniqueSurfaceKHR surface{};
	vk::Queue queue{};
	std::uint32_t queue_family{};

	std::unique_ptr<Shared> shared{};

	UniqueVma vma{};
	Swapchain swapchain{};
	RenderPass render_pass{};
	RenderFrames<> render_frames;
	DearImGui dear_imgui{};

	GraphicsDeviceInfo info{};

	vk::Result wait(vk::ArrayProxy<vk::Fence> const& fences) const { return device->waitForFences(fences, true, std::numeric_limits<std::uint64_t>::max()); }

	void reset(vk::Fence fence) const {
		wait(fence);
		device->resetFences(fence);
	}

	RenderTarget make_render_target(ImageView colour);
};

void gfx_create_device(VulkanDevice& out, GraphicsDeviceCreateInfo const& create_info);
void gfx_destroy_device(VulkanDevice& out);
inline GraphicsDeviceInfo const& gfx_info(VulkanDevice const& device) { return device.info; }
bool gfx_set_vsync(VulkanDevice& out, Vsync::Type vsync);
void gfx_render(VulkanDevice& out_device, GraphicsRenderer& out_renderer, glm::uvec2 framebuffer_extent);
} // namespace levk
