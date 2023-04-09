#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <glm/gtc/color_space.hpp>
#include <graphics/vulkan/ad_hoc_cmd.hpp>
#include <graphics/vulkan/device.hpp>
#include <graphics/vulkan/framebuffer.hpp>
#include <graphics/vulkan/image_barrier.hpp>
#include <graphics/vulkan/material.hpp>
#include <graphics/vulkan/pipeline.hpp>
#include <graphics/vulkan/primitive.hpp>
#include <graphics/vulkan/render_target.hpp>
#include <graphics/vulkan/shader.hpp>
#include <graphics/vulkan/texture.hpp>
#include <impl/frame_profiler.hpp>
#include <levk/asset/asset_providers.hpp>
#include <levk/asset/shader_provider.hpp>
#include <levk/graphics/material.hpp>
#include <levk/graphics/shader.hpp>
#include <levk/graphics/shader_buffer.hpp>
#include <levk/graphics/texture.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/error.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/visitor.hpp>
#include <levk/util/zip_ranges.hpp>
#include <levk/window/window.hpp>
#include <window/glfw/window.hpp>
#include <ranges>

namespace levk::vulkan {
namespace {
constexpr std::string_view validation_layer_v = "VK_LAYER_KHRONOS_validation";

constexpr Vsync from(vk::PresentModeKHR const mode) {
	switch (mode) {
	case vk::PresentModeKHR::eMailbox: return Vsync::eMailbox;
	case vk::PresentModeKHR::eImmediate: return Vsync::eOff;
	case vk::PresentModeKHR::eFifoRelaxed: return Vsync::eAdaptive;
	default: return Vsync::eOn;
	}
}

[[maybe_unused]] constexpr AntiAliasing from(vk::SampleCountFlagBits const samples) {
	switch (samples) {
	case vk::SampleCountFlagBits::e32: return AntiAliasing::e32x;
	case vk::SampleCountFlagBits::e16: return AntiAliasing::e16x;
	case vk::SampleCountFlagBits::e8: return AntiAliasing::e8x;
	case vk::SampleCountFlagBits::e4: return AntiAliasing::e4x;
	case vk::SampleCountFlagBits::e2: return AntiAliasing::e2x;
	default: return AntiAliasing::e1x;
	}
}

constexpr vk::SampleCountFlagBits from(AntiAliasing const aa) {
	switch (aa) {
	case AntiAliasing::e32x: return vk::SampleCountFlagBits::e32;
	case AntiAliasing::e16x: return vk::SampleCountFlagBits::e16;
	case AntiAliasing::e8x: return vk::SampleCountFlagBits::e8;
	case AntiAliasing::e4x: return vk::SampleCountFlagBits::e4;
	case AntiAliasing::e2x: return vk::SampleCountFlagBits::e2;
	default: return vk::SampleCountFlagBits::e1;
	}
}

constexpr vk::PresentModeKHR from(Vsync const type) {
	switch (type) {
	case Vsync::eMailbox: return vk::PresentModeKHR::eMailbox;
	case Vsync::eOff: return vk::PresentModeKHR::eImmediate;
	case Vsync::eAdaptive: return vk::PresentModeKHR::eFifoRelaxed;
	default: return vk::PresentModeKHR::eFifo;
	}
}

constexpr vk::Extent2D scaled(vk::Extent2D const extent, float scale) {
	glm::vec2 const in = glm::uvec2{extent.width, extent.height};
	glm::uvec2 const ret = scale * in;
	return {ret.x, ret.y};
}

constexpr std::uint32_t image_count(vk::SurfaceCapabilitiesKHR const& caps) noexcept {
	if (caps.maxImageCount < caps.minImageCount) { return std::max(3u, caps.minImageCount); }
	return std::clamp(3u, caps.minImageCount, caps.maxImageCount);
}

constexpr vk::Extent2D image_extent(vk::SurfaceCapabilitiesKHR const& caps, vk::Extent2D const fb) noexcept {
	constexpr auto limitless_v = std::numeric_limits<std::uint32_t>::max();
	if (caps.currentExtent.width < limitless_v && caps.currentExtent.height < limitless_v) { return caps.currentExtent; }
	auto const x = std::clamp(fb.width, caps.minImageExtent.width, caps.maxImageExtent.width);
	auto const y = std::clamp(fb.height, caps.minImageExtent.height, caps.maxImageExtent.height);
	return vk::Extent2D{x, y};
}

constexpr auto get_samples(AntiAliasingFlags supported, AntiAliasing const desired) {
	if (desired >= AntiAliasing::e32x && (supported.test(AntiAliasing::e32x))) { return AntiAliasing::e32x; }
	if (desired >= AntiAliasing::e16x && (supported.test(AntiAliasing::e16x))) { return AntiAliasing::e16x; }
	if (desired >= AntiAliasing::e8x && (supported.test(AntiAliasing::e8x))) { return AntiAliasing::e8x; }
	if (desired >= AntiAliasing::e4x && (supported.test(AntiAliasing::e4x))) { return AntiAliasing::e4x; }
	if (desired >= AntiAliasing::e2x && (supported.test(AntiAliasing::e2x))) { return AntiAliasing::e2x; }
	return AntiAliasing::e1x;
}

[[maybe_unused]] constexpr std::string_view vsync_status(vk::PresentModeKHR const mode) {
	switch (mode) {
	case vk::PresentModeKHR::eFifo: return "On";
	case vk::PresentModeKHR::eFifoRelaxed: return "Adaptive";
	case vk::PresentModeKHR::eImmediate: return "Off";
	case vk::PresentModeKHR::eMailbox: return "Double-buffered";
	default: return "Unsupported";
	}
}

constexpr VsyncFlags make_vsync(std::span<vk::PresentModeKHR const> supported_modes) {
	auto ret = VsyncFlags{};
	for (auto const mode : supported_modes) { ret.set(from(mode)); }
	return ret;
}

constexpr AntiAliasingFlags make_aa(vk::PhysicalDeviceProperties const& props) {
	auto ret = AntiAliasingFlags{};
	auto const supported = props.limits.framebufferColorSampleCounts;
	if (supported & vk::SampleCountFlagBits::e32) { ret.set(AntiAliasing::e32x); }
	if (supported & vk::SampleCountFlagBits::e16) { ret.set(AntiAliasing::e16x); }
	if (supported & vk::SampleCountFlagBits::e8) { ret.set(AntiAliasing::e8x); }
	if (supported & vk::SampleCountFlagBits::e4) { ret.set(AntiAliasing::e4x); }
	if (supported & vk::SampleCountFlagBits::e2) { ret.set(AntiAliasing::e2x); }
	if (supported & vk::SampleCountFlagBits::e1) { ret.set(AntiAliasing::e1x); }
	return ret;
}

vk::Format depth_format(vk::PhysicalDevice const gpu) {
	static constexpr auto target{vk::Format::eD32Sfloat};
	auto const props = gpu.getFormatProperties(target);
	if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) { return target; }
	return vk::Format::eD16Unorm;
}

vk::UniqueInstance make_instance(std::vector<char const*> extensions, RenderDeviceCreateInfo const& gdci, RenderDeviceInfo& out) {
	auto dl = vk::DynamicLoader{};
	VULKAN_HPP_DEFAULT_DISPATCHER.init(dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));
	if (gdci.validation) {
		auto const available_layers = vk::enumerateInstanceLayerProperties();
		auto layer_search = [](vk::LayerProperties const& lp) { return lp.layerName == validation_layer_v; };
		if (std::ranges::find_if(available_layers, layer_search) != available_layers.end()) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			out.validation = true;
		} else {
			logger::warn("[RenderDevice] Validation layer requested but not found");
			out.validation = false;
		}
	}

	auto const version = VK_MAKE_VERSION(0, 1, 0);
	auto const ai = vk::ApplicationInfo{"levk", version, "levk", version, VK_API_VERSION_1_3};
	auto ici = vk::InstanceCreateInfo{};
	ici.pApplicationInfo = &ai;
	out.portability = false;
#if defined(__APPLE__)
	ici.flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
	out.portability = true;
#endif
	ici.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
	ici.ppEnabledExtensionNames = extensions.data();
	if (out.validation) {
		static constexpr char const* layers[] = {validation_layer_v.data()};
		ici.enabledLayerCount = 1;
		ici.ppEnabledLayerNames = layers;
	}
	auto ret = vk::UniqueInstance{};
	try {
		ret = vk::createInstanceUnique(ici);
	} catch (vk::LayerNotPresentError const& e) {
		logger::error("[RenderDevice] {}", e.what());
		ici.enabledLayerCount = 0;
		ret = vk::createInstanceUnique(ici);
	}
	VULKAN_HPP_DEFAULT_DISPATCHER.init(ret.get());
	return ret;
}

vk::UniqueDebugUtilsMessengerEXT make_debug_messenger(vk::Instance instance) {
	auto validationCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT,
								 VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData, void*) -> vk::Bool32 {
		auto const msg = fmt::format("[vk] {}", pCallbackData && pCallbackData->pMessage ? pCallbackData->pMessage : "UNKNOWN");
		switch (messageSeverity) {
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: logger::error("{}", msg); break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: logger::warn("{}", msg); break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: logger::info("{}", msg); break;
		default: break;
		}
		return false;
	};

	auto dumci = vk::DebugUtilsMessengerCreateInfoEXT{};
	using vksev = vk::DebugUtilsMessageSeverityFlagBitsEXT;
	dumci.messageSeverity = vksev::eError | vksev::eWarning | vksev::eInfo;
	using vktype = vk::DebugUtilsMessageTypeFlagBitsEXT;
	dumci.messageType = vktype::eGeneral | vktype::ePerformance | vktype::eValidation;
	dumci.pfnUserCallback = validationCallback;
	return instance.createDebugUtilsMessengerEXTUnique(dumci, nullptr);
}

Gpu select_gpu(vk::Instance const instance, vk::SurfaceKHR const surface) {
	struct Entry {
		Gpu gpu{};
		int rank{};
		auto operator<=>(Entry const& rhs) const { return rank <=> rhs.rank; }
	};
	auto const get_queue_family = [surface](vk::PhysicalDevice const& device, std::uint32_t& out_family) {
		static constexpr auto queue_flags_v = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eTransfer;
		auto const properties = device.getQueueFamilyProperties();
		for (std::size_t i = 0; i < properties.size(); ++i) {
			auto const family = static_cast<std::uint32_t>(i);
			if (!device.getSurfaceSupportKHR(family, surface)) { continue; }
			if (!(properties[i].queueFlags & queue_flags_v)) { continue; }
			out_family = family;
			return true;
		}
		return false;
	};
	auto const devices = instance.enumeratePhysicalDevices();
	auto entries = std::vector<Entry>{};
	for (auto const& device : devices) {
		auto entry = Entry{.gpu = {device}};
		entry.gpu.properties = device.getProperties();
		if (!get_queue_family(device, entry.gpu.queue_family)) { continue; }
		if (entry.gpu.properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) { entry.rank -= 100; }
		entries.push_back(std::move(entry));
	}
	if (entries.empty()) { return {}; }
	std::sort(entries.begin(), entries.end());
	return std::move(entries.front().gpu);
}

vk::UniqueDevice make_device(std::span<char const* const> layers, Gpu const& gpu) {
	static constexpr float priority_v = 1.0f;
	static constexpr std::array required_extensions_v = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_MAINTENANCE1_EXTENSION_NAME,

#if defined(__APPLE__)
		"VK_KHR_portability_subset",
#endif
	};
	static constexpr std::array desired_extensions_v = {
		VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
	};

	auto qci = vk::DeviceQueueCreateInfo{{}, gpu.queue_family, 1, &priority_v};
	auto dci = vk::DeviceCreateInfo{};
	auto enabled = vk::PhysicalDeviceFeatures{};
	auto available_features = gpu.device.getFeatures();
	enabled.fillModeNonSolid = available_features.fillModeNonSolid;
	enabled.wideLines = available_features.wideLines;
	enabled.samplerAnisotropy = available_features.samplerAnisotropy;
	enabled.sampleRateShading = available_features.sampleRateShading;
	auto extensions = FlexArray<char const*, 8>{};
	auto const available_extensions = gpu.device.enumerateDeviceExtensionProperties();
	for (auto const* ext : required_extensions_v) {
		auto const found = [ext](vk::ExtensionProperties const& e) { return std::string_view{e.extensionName} == ext; };
		if (std::ranges::find_if(available_extensions, found) == available_extensions.end()) {
			throw Error{fmt::format("Required extension [{}] not supported by selected GPU [{}]", ext, gpu.properties.deviceName)};
		}
		extensions.insert(ext);
	}
	for (auto const* ext : desired_extensions_v) {
		auto const found = [ext](vk::ExtensionProperties const& e) { return std::string_view{e.extensionName} == ext; };
		if (std::ranges::find_if(available_extensions, found) != available_extensions.end()) { extensions.insert(ext); }
	}

	auto dynamic_rendering_feature = vk::PhysicalDeviceDynamicRenderingFeatures{true};
	auto synchronization_2_feature = vk::PhysicalDeviceSynchronization2FeaturesKHR{true};
	synchronization_2_feature.pNext = &dynamic_rendering_feature;

	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	dci.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
	dci.ppEnabledLayerNames = layers.data();
	dci.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
	dci.ppEnabledExtensionNames = extensions.span().data();
	dci.pEnabledFeatures = &enabled;
	dci.pNext = &synchronization_2_feature;

	auto ret = gpu.device.createDeviceUnique(dci);
	if (ret) { VULKAN_HPP_DEFAULT_DISPATCHER.init(ret.get()); }
	return ret;
}

vk::PresentModeKHR ideal_present_mode(std::unordered_set<vk::PresentModeKHR> const& available, vk::PresentModeKHR desired) {
	if (available.contains(desired)) { return desired; }
	if (available.contains(vk::PresentModeKHR::eFifoRelaxed)) { return vk::PresentModeKHR::eFifoRelaxed; }
	assert(available.contains(vk::PresentModeKHR::eFifo));
	return vk::PresentModeKHR::eFifo;
}

struct Swapchain {
	struct CreateInfo {
		Device::View device;

		ColourSpace colour_space{ColourSpace::eSrgb};
		Vsync vsync{Vsync::eAdaptive};
	};

	struct Formats {
		std::vector<vk::Format> srgb{};
		std::vector<vk::Format> linear{};

		static Formats make(std::span<vk::SurfaceFormatKHR const> available) {
			auto ret = Formats{};
			for (auto const format : available) {
				if (format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
					if (std::ranges::find(srgb_formats_v, format.format) != std::ranges::end(srgb_formats_v)) {
						ret.srgb.push_back(format.format);
					} else {
						ret.linear.push_back(format.format);
					}
				}
			}
			return ret;
		}
	};

	struct Storage {
		std::vector<ImageView> images{};
		std::vector<vk::UniqueImageView> views{};
		vk::UniqueSwapchainKHR swapchain{};
		glm::uvec2 extent{};
		std::optional<std::size_t> image_index{};
	};

	Formats formats{};
	std::unordered_set<vk::PresentModeKHR> modes{};
	Device::View device{};
	vk::SwapchainCreateInfoKHR info{};

	Storage storage{};

	static Swapchain make(CreateInfo const& create_info) {
		auto ret = Swapchain{};
		auto const supported_modes = create_info.device.gpu->device.getSurfacePresentModesKHR(create_info.device.surface);
		ret.device = create_info.device;
		ret.modes = {supported_modes.begin(), supported_modes.end()};
		ret.formats = Formats::make(create_info.device.gpu->device.getSurfaceFormatsKHR(create_info.device.surface));
		auto const present_mode = ideal_present_mode(ret.modes, from(create_info.vsync));
		ret.info = ret.make_swci(create_info.colour_space, present_mode);
		return ret;
	}

	vk::SurfaceFormatKHR surface_format(ColourSpace colour_space) const {
		if (colour_space == ColourSpace::eSrgb && !formats.srgb.empty()) { return formats.srgb.front(); }
		return formats.linear.front();
	}

	vk::SwapchainCreateInfoKHR make_swci(ColourSpace colour_space, vk::PresentModeKHR mode) const {
		auto ret = vk::SwapchainCreateInfoKHR{};
		auto const format = surface_format(colour_space);
		ret.surface = device.surface;
		ret.presentMode = mode;
		ret.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;
		ret.queueFamilyIndexCount = 1u;
		ret.pQueueFamilyIndices = &device.gpu->queue_family;
		ret.imageColorSpace = format.colorSpace;
		ret.imageArrayLayers = 1u;
		ret.imageFormat = format.format;
		return ret;
	}

	void refresh(std::optional<glm::uvec2> extent = {}, std::optional<Vsync> vsync = {}) {
		auto const caps = device.gpu->device.getSurfaceCapabilitiesKHR(device.surface);
		if (extent) {
			info.imageExtent = image_extent(caps, vk::Extent2D{extent->x, extent->y});
			storage.extent = {info.imageExtent.width, info.imageExtent.height};
		}
		if (vsync) {
			assert(modes.contains(from(*vsync)));
			info.presentMode = from(*vsync);
		}
		assert(info.imageExtent.width > 0 && info.imageExtent.height > 0);
		auto create_info = info;
		create_info.minImageCount = image_count(caps);
		create_info.oldSwapchain = storage.swapchain.get();
		auto vk_swapchain = vk::SwapchainKHR{};
		auto const ret = device.device.createSwapchainKHR(&create_info, nullptr, &vk_swapchain);
		if (ret != vk::Result::eSuccess) { throw Error{"Failed to create Vulkan Swapchain"}; }

		info = std::move(create_info);
		if (storage.swapchain) { device.defer->push(std::move(storage)); }
		storage.swapchain = vk::UniqueSwapchainKHR{vk_swapchain, device.device};
		auto count = std::uint32_t{};
		if (device.device.getSwapchainImagesKHR(*storage.swapchain, &count, nullptr) != vk::Result::eSuccess) { throw Error{"Failed to get Swapchain Images"}; }
		storage.images.resize(count);
		auto const images = device.device.getSwapchainImagesKHR(*storage.swapchain);
		storage.images.clear();
		storage.views.clear();
		for (auto const image : images) {
			storage.views.push_back(device.vma.make_image_view(image, info.imageFormat));
			storage.images.push_back({
				.image = image,
				.view = *storage.views.back(),
				.extent = info.imageExtent,
				.format = info.imageFormat,
			});
		}

		logger::info("[RenderDevice] Swapchain extent: [{}x{}] | images: [{}] | colour space: [{}] | vsync: [{}]", create_info.imageExtent.width,
					 create_info.imageExtent.height, storage.images.size(), is_srgb(info.imageFormat) ? "sRGB" : "linear", vsync_status(info.presentMode));
	}

	std::optional<ImageView> acquire(glm::uvec2 extent, vk::Semaphore semaphore, vk::Fence fence = {}) {
		assert(!storage.image_index);
		auto index = std::uint32_t{};
		auto const result = device.queue->with([&](vk::Queue) {
			return device.device.acquireNextImageKHR(*storage.swapchain, std::numeric_limits<std::uint64_t>::max(), semaphore, fence, &index);
		});
		switch (result) {
		case vk::Result::eSuccess: storage.image_index = index; break;
		case vk::Result::eSuboptimalKHR:
		case vk::Result::eErrorOutOfDateKHR: refresh(extent); break;
		default: break;
		}
		if (storage.image_index) { return storage.images[static_cast<std::size_t>(*storage.image_index)]; }
		return {};
	}

	bool present(glm::uvec2 extent, vk::Semaphore wait) {
		assert(storage.image_index);
		auto pi = vk::PresentInfoKHR{};
		auto const image_index = static_cast<std::uint32_t>(*storage.image_index);
		pi.pWaitSemaphores = &wait;
		pi.waitSemaphoreCount = 1u;
		pi.pImageIndices = &image_index;
		pi.pSwapchains = &*storage.swapchain;
		pi.swapchainCount = 1u;
		auto const result = device.queue->with([pi](vk::Queue queue) { return queue.presentKHR(&pi); });
		storage.image_index.reset();
		switch (result) {
		case vk::Result::eSuccess: return true;
		case vk::Result::eSuboptimalKHR:
		case vk::Result::eErrorOutOfDateKHR: refresh(extent); break;
		default: break;
		}
		return false;
	}
};

struct RenderSync {
	vk::UniqueSemaphore draw{};
	vk::UniqueSemaphore present{};
	vk::UniqueFence drawn{};

	struct View {
		vk::Semaphore draw{};
		vk::Semaphore present{};
		vk::Fence drawn{};
	};

	static RenderSync make(Device::View const& device) {
		return RenderSync{
			.draw = device.device.createSemaphoreUnique({}),
			.present = device.device.createSemaphoreUnique({}),
			.drawn = device.device.createFenceUnique({vk::FenceCreateFlagBits::eSignaled}),
		};
	}

	View view() const {
		return View{
			.draw = *draw,
			.present = *present,
			.drawn = *drawn,
		};
	}
};

struct DearImGui {
	enum class State { eNewFrame, eEndFrame };

	vk::UniqueDescriptorPool pool{};
	State state{};

	static DearImGui make(glfw::Window const& window, DeviceView const& device, vk::Format colour, vk::Format depth) {
		auto ret = DearImGui{};
		vk::DescriptorPoolSize pool_sizes[] = {
			{vk::DescriptorType::eSampledImage, 1000},		   {vk::DescriptorType::eCombinedImageSampler, 1000},
			{vk::DescriptorType::eSampledImage, 1000},		   {vk::DescriptorType::eStorageImage, 1000},
			{vk::DescriptorType::eUniformTexelBuffer, 1000},   {vk::DescriptorType::eStorageTexelBuffer, 1000},
			{vk::DescriptorType::eUniformBuffer, 1000},		   {vk::DescriptorType::eStorageBuffer, 1000},
			{vk::DescriptorType::eUniformBufferDynamic, 1000}, {vk::DescriptorType::eStorageBufferDynamic, 1000},
			{vk::DescriptorType::eInputAttachment, 1000},
		};
		auto dpci = vk::DescriptorPoolCreateInfo{};
		dpci.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
		dpci.maxSets = 1000 * std::size(pool_sizes);
		dpci.poolSizeCount = static_cast<std::uint32_t>(std::size(pool_sizes));
		dpci.pPoolSizes = pool_sizes;
		ret.pool = device.device.createDescriptorPoolUnique(dpci);

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

		ImGui::StyleColorsDark();
		if (is_srgb(colour)) {
			auto* colours = ImGui::GetStyle().Colors;
			for (int i = 0; i < ImGuiCol_COUNT; ++i) {
				auto& colour = colours[i];
				auto const corrected = glm::convertSRGBToLinear(glm::vec4{colour.x, colour.y, colour.z, colour.w});
				colour = {corrected.x, corrected.y, corrected.z, corrected.w};
			}
		}

		auto loader = vk::DynamicLoader{};
		auto get_fn = [&loader](char const* name) { return loader.getProcAddress<PFN_vkVoidFunction>(name); };
		auto lambda = +[](char const* name, void* ud) {
			auto const* gf = reinterpret_cast<decltype(get_fn)*>(ud);
			return (*gf)(name);
		};
		ImGui_ImplVulkan_LoadFunctions(lambda, &get_fn);
		ImGui_ImplGlfw_InitForVulkan(window.window, true);
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = device.instance;
		init_info.PhysicalDevice = device.gpu->device;
		init_info.Device = device.device;
		init_info.QueueFamily = device.gpu->queue_family;
		init_info.Queue = device.queue->queue;
		init_info.DescriptorPool = *ret.pool;
		init_info.Subpass = 0;
		init_info.MinImageCount = 2;
		init_info.ImageCount = 2;
		init_info.MSAASamples = static_cast<VkSampleCountFlagBits>(vk::SampleCountFlagBits::e1);

		ImGui_ImplVulkan_Init(&init_info, static_cast<VkFormat>(colour), static_cast<VkFormat>(depth));

		{
			auto cmd = AdHocCmd{device};
			ImGui_ImplVulkan_CreateFontsTexture(cmd.cb);
		}
		ImGui_ImplVulkan_DestroyFontUploadObjects();

		return ret;
	}

	void new_frame() {
		if (state == State::eEndFrame) { end_frame(); }
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		state = State::eEndFrame;
	}

	void end_frame() {
		// ImGui::Render calls ImGui::EndFrame
		if (state == State::eNewFrame) { new_frame(); }
		ImGui::Render();
		state = State::eNewFrame;
	}

	void render(vk::CommandBuffer cb) {
		if (state == State::eEndFrame) { end_frame(); }
		if (auto* data = ImGui::GetDrawData()) { ImGui_ImplVulkan_RenderDrawData(data, cb); }
	}
};

struct RenderCb {
	vk::CommandBuffer cb_shadow{};
	vk::CommandBuffer cb_3d{};
	vk::CommandBuffer cb_ui{};
};

struct Waiter {
	DeviceView device{};
	~Waiter() {
		device.device.waitIdle();
		device.defer->clear();
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	Waiter& operator=(Waiter&&) = delete;
};
} // namespace

struct Device::Impl {
	Ptr<glfw::Window> window{};
	Gpu gpu{};
	Swapchain swapchain{};
	std::array<RenderSync, buffering_v<>> render_sync{};
	PipelineStorage pipeline_storage{};
	SamplerStorage sampler_storage{};

	CommandAllocator cmd_allocator{};

	Buffered<SetAllocator> set_allocators{};
	Buffered<ScratchBufferAllocator> scratch_buffer_allocators{};

	Buffered<RenderCb> render_cbs{};
	DepthTarget rt_shadow{};
	RenderTarget rt_3d{};
	RenderTarget rt_ui{};

	DearImGui dear_imgui{};
	DeferQueue defer{};

	Index buffered_index{};
	RenderMode default_render_mode{};
	std::uint64_t draw_calls{};
	Waiter waiter{};
};

void Device::Deleter::operator()(Impl const* ptr) const { delete ptr; }

Device::Device(Window const& window, RenderDeviceCreateInfo const& create_info) {
	auto* glfw_window = window.glfw_window();
	if (!glfw_window) { throw Error{"Invalid Window instance"}; }

	instance = make_instance(glfw_window->vulkan_extensions(), create_info, device_info);

	if (device_info.validation) { debug = make_debug_messenger(*instance); }

	surface = glfw_window->make_surface(*instance);
	if (!surface) { throw Error{"Failed to create Vulkan Surface"}; }

	impl = std::unique_ptr<Impl, Deleter>(new Impl{.window = glfw_window});
	impl->gpu = select_gpu(*instance, *surface);

	auto layers = FlexArray<char const*, 2>{};
	if (create_info.validation) { layers.insert(validation_layer_v.data()); }
	device = make_device(layers.span(), impl->gpu);
	Queue::make(queue, *device, impl->gpu.queue_family);

	vma = Vma::make(*instance, impl->gpu.device, *device);

	auto const view_ = view();
	auto const sci = Swapchain::CreateInfo{
		.device = view_,
		.colour_space = create_info.swapchain,
		.vsync = create_info.vsync,
	};
	impl->swapchain = Swapchain::make(sci);
	impl->swapchain.refresh(impl->window->framebuffer_extent());

	for (auto& sync : impl->render_sync) { sync = RenderSync::make(view_); }
	for (auto& buffer : impl->scratch_buffer_allocators) { buffer.vma = vma.get(); }
	for (auto& set_allocator : impl->set_allocators) { set_allocator.device = *device; }

	auto const dtci = DepthTarget::CreateInfo{
		.extent = {device_info.shadow_map_resolution.x, device_info.shadow_map_resolution.y},
		.format = depth_format(impl->gpu.device),
	};
	impl->rt_shadow = DepthTarget::make(view_, dtci);

	auto const rtci = RenderTarget::CreateInfo{
		.extent = impl->swapchain.info.imageExtent,
		.colour = impl->swapchain.info.imageFormat,
		.depth = dtci.format,
		.samples = from(device_info.current_aa),
	};
	impl->rt_3d = RenderTarget::make_off_screen(view_, rtci);

	auto const flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient;
	impl->cmd_allocator = CommandAllocator::make(*device, impl->gpu.queue_family, flags);
	for (auto& cb : impl->render_cbs) {
		auto cbs = std::array<vk::CommandBuffer, 3>{};
		impl->cmd_allocator.allocate(cbs);
		cb.cb_shadow = cbs[0];
		cb.cb_3d = cbs[1];
		cb.cb_ui = cbs[2];
	}

	impl->dear_imgui = DearImGui::make(*glfw_window, view_, rtci.colour, {});
	impl->dear_imgui.new_frame();

	device_info.supported_vsync = make_vsync(impl->gpu.device.getSurfacePresentModesKHR(*surface));
	device_info.current_vsync = from(impl->swapchain.info.presentMode);
	device_info.supported_aa = make_aa(impl->gpu.properties);
	device_info.current_aa = get_samples(device_info.supported_aa, create_info.anti_aliasing);
	device_info.name = impl->gpu.properties.deviceName;

	impl->waiter.device = view_;
}

std::uint64_t Device::draw_calls() const { return impl->draw_calls; }

bool Device::set_vsync(Vsync desired) {
	if (!device_info.supported_vsync.test(desired)) { return false; }
	impl->swapchain.refresh({}, desired);
	device_info.current_vsync = desired;
	return true;
}

bool Device::render(Renderer& renderer, AssetProviders const& asset_providers) {
	assert(impl);

	auto const framebuffer_extent = impl->window->framebuffer_extent();
	if (framebuffer_extent.x == 0 || framebuffer_extent.y == 0) { return false; }

	FrameProfiler::instance().profile(FrameProfile::Type::eAcquireFrame);
	auto sync = impl->render_sync[impl->buffered_index].view();
	auto acquired = impl->swapchain.acquire(framebuffer_extent, sync.draw);
	if (!acquired) { return false; }

	if (device->waitForFences(sync.drawn, true, std::numeric_limits<std::uint64_t>::max()) != vk::Result::eSuccess) { return false; }
	device->resetFences(sync.drawn);

	impl->draw_calls = {};
	impl->scratch_buffer_allocators[impl->buffered_index].clear();
	impl->set_allocators[impl->buffered_index].reset_all();
	renderer.asset_providers = &asset_providers;
	auto render_cb = impl->render_cbs[impl->buffered_index];

	FrameProfiler::instance().profile(FrameProfile::Type::eRenderShadowMap);
	auto fb_shadow = impl->rt_shadow.depthbuffer();
	render_cb.cb_shadow.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
	fb_shadow.undef_to_optimal(render_cb.cb_shadow);
	fb_shadow.begin_render(render_cb.cb_shadow);
	renderer.render_shadow(render_cb.cb_shadow, fb_shadow, device_info.shadow_map_world_size);
	fb_shadow.end_render(render_cb.cb_shadow);
	fb_shadow.optimal_to_read_only(render_cb.cb_shadow);
	render_cb.cb_shadow.end();

	FrameProfiler::instance().profile(FrameProfile::Type::eRender3D);
	auto fb_3d = impl->rt_3d.refresh(scaled(impl->swapchain.info.imageExtent, device_info.render_scale));
	render_cb.cb_3d.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
	fb_3d.undef_to_optimal(render_cb.cb_3d);
	fb_3d.begin_render(device_info.clear_colour.to_vec4(), render_cb.cb_3d);
	renderer.render_3d(render_cb.cb_3d, fb_3d, fb_shadow.image);
	fb_3d.end_render(render_cb.cb_3d);
	fb_3d.optimal_to_read_only(render_cb.cb_3d);
	render_cb.cb_3d.end();

	FrameProfiler::instance().profile(FrameProfile::Type::eRenderUI);
	auto fb_ui = Framebuffer{.colour = *acquired};
	render_cb.cb_ui.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
	fb_ui.undef_to_optimal(render_cb.cb_ui);
	fb_ui.begin_render(device_info.clear_colour.to_vec4(), render_cb.cb_ui);
	renderer.render_ui(render_cb.cb_ui, fb_ui, fb_3d.output());
	impl->dear_imgui.render(render_cb.cb_ui);
	fb_ui.end_render(render_cb.cb_ui);
	fb_ui.optimal_to_present(render_cb.cb_ui);
	render_cb.cb_ui.end();

	FrameProfiler::instance().profile(FrameProfile::Type::eRenderSubmit);
	auto const wsi = vk::SemaphoreSubmitInfo{sync.draw, {}, vk::PipelineStageFlagBits2::eColorAttachmentOutput};
	auto const cbis = std::array<vk::CommandBufferSubmitInfo, 3>{render_cb.cb_shadow, render_cb.cb_3d, render_cb.cb_ui};
	auto const ssi = vk::SemaphoreSubmitInfo{sync.present, {}, vk::PipelineStageFlagBits2::eColorAttachmentOutput};
	auto submit_info = vk::SubmitInfo2{{}, wsi, cbis, ssi};
	queue.with([&](vk::Queue queue) { queue.submit2(submit_info, sync.drawn); });

	FrameProfiler::instance().profile(FrameProfile::Type::eRenderPresent);
	auto const ret = impl->swapchain.present(framebuffer_extent, sync.present);

	FrameProfiler::instance().finish();

	impl->buffered_index.next();
	impl->dear_imgui.new_frame();
	impl->defer.next();

	return ret;
}

auto Device::view() -> View {
	return View{
		.instance = *instance,
		.surface = *surface,
		.device = *device,
		.gpu = &impl->gpu,
		.vma = vma.get(),
		.default_render_mode = impl->default_render_mode,
		.queue = &queue,
		.defer = &impl->defer,
		.set_allocator = &impl->set_allocators[impl->buffered_index],
		.scratch_buffer_allocator = &impl->scratch_buffer_allocators[impl->buffered_index],
		.pipeline_storage = &impl->pipeline_storage,
		.sampler_storage = &impl->sampler_storage,
		.buffered_index = &impl->buffered_index,
		.draw_calls = &impl->draw_calls,
	};
}
} // namespace levk::vulkan

bool levk::vulkan::is_srgb(vk::Format format) { return std::ranges::find(srgb_formats_v, format) != std::ranges::end(srgb_formats_v); }
