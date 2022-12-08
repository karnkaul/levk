#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <glm/gtc/color_space.hpp>
#include <glm/mat4x4.hpp>
#include <levk/impl/desktop_window.hpp>
#include <levk/impl/vulkan_device.hpp>
#include <levk/impl/vulkan_surface.hpp>
#include <levk/util/error.hpp>
#include <levk/util/flex_array.hpp>
#include <levk/util/logger.hpp>
#include <levk/window.hpp>

namespace {
using namespace std::string_view_literals;
namespace logger = levk::logger;

constexpr auto validation_layer_v = "VK_LAYER_KHRONOS_validation"sv;

vk::UniqueInstance make_instance(std::vector<char const*> extensions, levk::GraphicsDeviceCreateInfo const& gdci, levk::GraphicsDeviceInfo& out) {
	auto dl = vk::DynamicLoader{};
	VULKAN_HPP_DEFAULT_DISPATCHER.init(dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));
	if (gdci.validation) {
		auto const available_layers = vk::enumerateInstanceLayerProperties();
		auto layer_search = [](vk::LayerProperties const& lp) { return lp.layerName == validation_layer_v; };
		if (std::find_if(available_layers.begin(), available_layers.end(), layer_search) != available_layers.end()) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			out.validation = true;
		} else {
			logger::warn("[Device] Validation layer requested but not found");
			out.validation = false;
		}
	}

	auto const version = VK_MAKE_VERSION(0, 1, 0);
	auto const ai = vk::ApplicationInfo{"gltf", version, "gltf", version, VK_API_VERSION_1_1};
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
		logger::error("[Device] {}", e.what());
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

levk::Gpu select_gpu(vk::Instance const instance, vk::SurfaceKHR const surface) {
	struct Entry {
		levk::Gpu gpu{};
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

vk::UniqueDevice make_device(std::span<char const* const> layers, levk::Gpu const& gpu) {
	static constexpr float priority_v = 1.0f;
	static constexpr std::array required_extensions_v = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_MAINTENANCE1_EXTENSION_NAME,
#if defined(__APPLE__)
		"VK_KHR_portability_subset",
#endif
	};

	auto qci = vk::DeviceQueueCreateInfo{{}, gpu.queue_family, 1, &priority_v};
	auto dci = vk::DeviceCreateInfo{};
	auto enabled = vk::PhysicalDeviceFeatures{};
	auto available = gpu.device.getFeatures();
	enabled.fillModeNonSolid = available.fillModeNonSolid;
	enabled.wideLines = available.wideLines;
	enabled.samplerAnisotropy = available.samplerAnisotropy;
	enabled.sampleRateShading = available.sampleRateShading;
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	dci.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
	dci.ppEnabledLayerNames = layers.data();
	dci.enabledExtensionCount = static_cast<std::uint32_t>(required_extensions_v.size());
	dci.ppEnabledExtensionNames = required_extensions_v.data();
	dci.pEnabledFeatures = &enabled;
	auto ret = gpu.device.createDeviceUnique(dci);
	if (ret) { VULKAN_HPP_DEFAULT_DISPATCHER.init(ret.get()); }
	return ret;
}

levk::UniqueVma make_vma(vk::Instance instance, vk::PhysicalDevice gpu, vk::Device device) {
	assert(instance && gpu && device);
	auto vaci = VmaAllocatorCreateInfo{};
	vaci.instance = instance;
	vaci.physicalDevice = gpu;
	vaci.device = device;
	auto dl = VULKAN_HPP_DEFAULT_DISPATCHER;
	auto vkFunc = VmaVulkanFunctions{};
	vkFunc.vkGetInstanceProcAddr = dl.vkGetInstanceProcAddr;
	vkFunc.vkGetDeviceProcAddr = dl.vkGetDeviceProcAddr;
	vaci.pVulkanFunctions = &vkFunc;
	auto ret = levk::UniqueVma{};
	if (vmaCreateAllocator(&vaci, &ret.get().allocator) != VK_SUCCESS) { throw levk::Error{"Failed to create Vulkan Memory Allocator"}; }
	ret.get().device = device;
	return ret;
}

levk::Swapchain::Formats make_swapchain_formats(std::span<vk::SurfaceFormatKHR const> available) {
	auto ret = levk::Swapchain::Formats{};
	for (auto const format : available) {
		if (format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			if (std::find(std::begin(levk::srgb_formats_v), std::end(levk::srgb_formats_v), format.format) != std::end(levk::srgb_formats_v)) {
				ret.srgb.push_back(format.format);
			} else {
				ret.linear.push_back(format.format);
			}
		}
	}
	return ret;
}

vk::SurfaceFormatKHR surface_format(levk::Swapchain::Formats const& formats, levk::ColourSpace colour_space) {
	if (colour_space == levk::ColourSpace::eSrgb && !formats.srgb.empty()) { return formats.srgb.front(); }
	return formats.linear.front();
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

vk::SwapchainCreateInfoKHR make_swci(std::uint32_t queue_family, vk::SurfaceKHR surface, levk::Swapchain::Formats const& formats,
									 levk::ColourSpace colour_space, vk::PresentModeKHR mode) {
	vk::SwapchainCreateInfoKHR ret;
	auto const format = surface_format(formats, colour_space);
	ret.surface = surface;
	ret.presentMode = mode;
	ret.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;
	ret.queueFamilyIndexCount = 1u;
	ret.pQueueFamilyIndices = &queue_family;
	ret.imageColorSpace = format.colorSpace;
	ret.imageArrayLayers = 1u;
	ret.imageFormat = format.format;
	return ret;
}

constexpr levk::Vsync::Type from(vk::PresentModeKHR const mode) {
	switch (mode) {
	case vk::PresentModeKHR::eMailbox: return levk::Vsync::eMailbox;
	case vk::PresentModeKHR::eImmediate: return levk::Vsync::eOff;
	case vk::PresentModeKHR::eFifoRelaxed: return levk::Vsync::eAdaptive;
	default: return levk::Vsync::eOn;
	}
}

[[maybe_unused]] constexpr levk::AntiAliasing::Type from(vk::SampleCountFlagBits const samples) {
	switch (samples) {
	case vk::SampleCountFlagBits::e32: return levk::AntiAliasing::e32x;
	case vk::SampleCountFlagBits::e16: return levk::AntiAliasing::e16x;
	case vk::SampleCountFlagBits::e8: return levk::AntiAliasing::e8x;
	case vk::SampleCountFlagBits::e4: return levk::AntiAliasing::e4x;
	case vk::SampleCountFlagBits::e2: return levk::AntiAliasing::e2x;
	default: return levk::AntiAliasing::Type::e1x;
	}
}

constexpr vk::SampleCountFlagBits from(levk::AntiAliasing::Type const aa) {
	switch (aa) {
	case levk::AntiAliasing::e32x: return vk::SampleCountFlagBits::e32;
	case levk::AntiAliasing::e16x: return vk::SampleCountFlagBits::e16;
	case levk::AntiAliasing::e8x: return vk::SampleCountFlagBits::e8;
	case levk::AntiAliasing::e4x: return vk::SampleCountFlagBits::e4;
	case levk::AntiAliasing::e2x: return vk::SampleCountFlagBits::e2;
	default: return vk::SampleCountFlagBits::e1;
	}
}

constexpr vk::PresentModeKHR from(levk::Vsync::Type const type) {
	switch (type) {
	case levk::Vsync::eMailbox: return vk::PresentModeKHR::eMailbox;
	case levk::Vsync::eOff: return vk::PresentModeKHR::eImmediate;
	case levk::Vsync::eAdaptive: return vk::PresentModeKHR::eFifoRelaxed;
	default: return vk::PresentModeKHR::eFifo;
	}
}

levk::Vsync make_vsync(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
	auto const supported_modes = device.getSurfacePresentModesKHR(surface);
	auto ret = levk::Vsync{};
	for (auto const mode : supported_modes) { ret.flags |= from(mode); }
	return ret;
}

levk::AntiAliasing make_aa(vk::PhysicalDeviceProperties const& props) {
	auto ret = levk::AntiAliasing{};
	auto const supported = props.limits.framebufferColorSampleCounts;
	if (supported & vk::SampleCountFlagBits::e32) { ret.flags |= levk::AntiAliasing::e32x; }
	if (supported & vk::SampleCountFlagBits::e16) { ret.flags |= levk::AntiAliasing::e16x; }
	if (supported & vk::SampleCountFlagBits::e8) { ret.flags |= levk::AntiAliasing::e8x; }
	if (supported & vk::SampleCountFlagBits::e4) { ret.flags |= levk::AntiAliasing::e4x; }
	if (supported & vk::SampleCountFlagBits::e2) { ret.flags |= levk::AntiAliasing::e2x; }
	if (supported & vk::SampleCountFlagBits::e1) { ret.flags |= levk::AntiAliasing::e1x; }
	return ret;
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

constexpr auto get_samples(levk::AntiAliasing supported, levk::AntiAliasing::Type const desired) {
	if (desired >= levk::AntiAliasing::e32x && (supported.flags & levk::AntiAliasing::e32x)) { return levk::AntiAliasing::e32x; }
	if (desired >= levk::AntiAliasing::e16x && (supported.flags & levk::AntiAliasing::e16x)) { return levk::AntiAliasing::e16x; }
	if (desired >= levk::AntiAliasing::e8x && (supported.flags & levk::AntiAliasing::e8x)) { return levk::AntiAliasing::e8x; }
	if (desired >= levk::AntiAliasing::e4x && (supported.flags & levk::AntiAliasing::e4x)) { return levk::AntiAliasing::e4x; }
	if (desired >= levk::AntiAliasing::e2x && (supported.flags & levk::AntiAliasing::e2x)) { return levk::AntiAliasing::e2x; }
	return levk::AntiAliasing::e1x;
}

vk::Format depth_format(vk::PhysicalDevice const gpu) {
	static constexpr auto target{vk::Format::eD32Sfloat};
	auto const props = gpu.getFormatProperties(target);
	if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) { return target; }
	return vk::Format::eD16Unorm;
}

vk::UniqueRenderPass make_render_pass(vk::Device device, vk::SampleCountFlagBits samples, vk::Format colour, vk::Format depth) {
	auto attachments = std::vector<vk::AttachmentDescription>{};
	auto colour_ref = vk::AttachmentReference{};
	auto depth_ref = vk::AttachmentReference{};
	auto resolve_ref = vk::AttachmentReference{};

	auto subpass = vk::SubpassDescription{};
	subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colour_ref;

	colour_ref.attachment = static_cast<std::uint32_t>(attachments.size());
	colour_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;
	auto attachment = vk::AttachmentDescription{};
	attachment.format = colour;
	attachment.samples = samples;
	attachment.loadOp = vk::AttachmentLoadOp::eClear;
	attachment.storeOp = samples > vk::SampleCountFlagBits::e1 ? vk::AttachmentStoreOp::eDontCare : vk::AttachmentStoreOp::eStore;
	attachment.initialLayout = attachment.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
	attachments.push_back(attachment);

	if (depth != vk::Format::eUndefined) {
		subpass.pDepthStencilAttachment = &depth_ref;
		depth_ref.attachment = static_cast<std::uint32_t>(attachments.size());
		depth_ref.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
		auto attachment = vk::AttachmentDescription{};
		attachment.format = depth;
		attachment.samples = samples;
		attachment.loadOp = vk::AttachmentLoadOp::eClear;
		attachment.storeOp = vk::AttachmentStoreOp::eDontCare;
		attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachment.initialLayout = attachment.finalLayout = depth_ref.layout;
		attachments.push_back(attachment);
	}

	if (samples > vk::SampleCountFlagBits::e1) {
		subpass.pResolveAttachments = &resolve_ref;
		resolve_ref.attachment = static_cast<std::uint32_t>(attachments.size());
		resolve_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

		attachment.samples = vk::SampleCountFlagBits::e1;
		attachment.loadOp = vk::AttachmentLoadOp::eDontCare;
		attachment.storeOp = vk::AttachmentStoreOp::eStore;
		attachments.push_back(attachment);
	}

	auto rpci = vk::RenderPassCreateInfo{};
	rpci.attachmentCount = static_cast<std::uint32_t>(attachments.size());
	rpci.pAttachments = attachments.data();
	rpci.subpassCount = 1u;
	rpci.pSubpasses = &subpass;
	auto sd = vk::SubpassDependency{};
	sd.srcSubpass = VK_SUBPASS_EXTERNAL;
	sd.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
	sd.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
	sd.srcStageMask = sd.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests;
	rpci.dependencyCount = 1u;
	rpci.pDependencies = &sd;
	return device.createRenderPassUnique(rpci);
}

levk::DearImGui make_imgui(levk::Window const& window, levk::VulkanDevice const& device) {
	auto ret = levk::DearImGui{};
	vk::DescriptorPoolSize pool_sizes[] = {
		{vk::DescriptorType::eSampledImage, 1000},		   {vk::DescriptorType::eCombinedImageSampler, 1000}, {vk::DescriptorType::eSampledImage, 1000},
		{vk::DescriptorType::eStorageImage, 1000},		   {vk::DescriptorType::eUniformTexelBuffer, 1000},	  {vk::DescriptorType::eStorageTexelBuffer, 1000},
		{vk::DescriptorType::eUniformBuffer, 1000},		   {vk::DescriptorType::eStorageBuffer, 1000},		  {vk::DescriptorType::eUniformBufferDynamic, 1000},
		{vk::DescriptorType::eStorageBufferDynamic, 1000}, {vk::DescriptorType::eInputAttachment, 1000},
	};
	auto dpci = vk::DescriptorPoolCreateInfo{};
	dpci.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
	dpci.maxSets = 1000 * std::size(pool_sizes);
	dpci.poolSizeCount = static_cast<std::uint32_t>(std::size(pool_sizes));
	dpci.pPoolSizes = pool_sizes;
	ret.pool = device.device->createDescriptorPoolUnique(dpci);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

	ImGui::StyleColorsDark();
	if (device.info.swapchain == levk::ColourSpace::eSrgb) {
		auto* colours = ImGui::GetStyle().Colors;
		for (int i = 0; i < ImGuiCol_COUNT; ++i) {
			auto& colour = colours[i];
			auto const corrected = glm::convertSRGBToLinear(glm::vec4{colour.x, colour.y, colour.z, colour.w});
			colour = {corrected.x, corrected.y, corrected.z, corrected.w};
		}
	}

	auto const* desktop_window = window.as<levk::DesktopWindow>();
	if (!desktop_window) { throw levk::Error{"TODO: error"}; }
	auto loader = vk::DynamicLoader{};
	auto get_fn = [&loader](char const* name) { return loader.getProcAddress<PFN_vkVoidFunction>(name); };
	auto lambda = +[](char const* name, void* ud) {
		auto const* gf = reinterpret_cast<decltype(get_fn)*>(ud);
		return (*gf)(name);
	};
	ImGui_ImplVulkan_LoadFunctions(lambda, &get_fn);
	ImGui_ImplGlfw_InitForVulkan(desktop_window->window, true);
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = *device.instance;
	init_info.PhysicalDevice = device.gpu.device;
	init_info.Device = *device.device;
	init_info.QueueFamily = device.queue_family;
	init_info.Queue = device.queue;
	init_info.DescriptorPool = *ret.pool;
	init_info.Subpass = 0;
	init_info.MinImageCount = 2;
	init_info.ImageCount = 2;
	init_info.MSAASamples = static_cast<VkSampleCountFlagBits>(from(device.info.current_aa));

	ImGui_ImplVulkan_Init(&init_info, *device.render_pass.render_pass);

	{
		auto cmd = levk::Cmd{device};
		ImGui_ImplVulkan_CreateFontsTexture(cmd.cb);
	}
	ImGui_ImplVulkan_DestroyFontUploadObjects();

	return ret;
}

vk::Result make_swapchain(levk::VulkanDevice& out, std::optional<glm::uvec2> extent, std::optional<vk::PresentModeKHR> mode) {
	auto create_info = out.swapchain.info;
	if (mode) {
		assert(out.swapchain.modes.contains(*mode));
		create_info.presentMode = *mode;
	}
	auto const caps = out.gpu.device.getSurfaceCapabilitiesKHR(*out.surface);
	if (extent) { create_info.imageExtent = image_extent(caps, vk::Extent2D{extent->x, extent->y}); }

	create_info.minImageCount = image_count(caps);
	create_info.oldSwapchain = out.swapchain.storage.swapchain.get();
	auto vk_swapchain = vk::SwapchainKHR{};
	auto const ret = out.device->createSwapchainKHR(&create_info, nullptr, &vk_swapchain);
	if (ret != vk::Result::eSuccess) { return ret; }
	out.swapchain.info = std::move(create_info);
	out.shared->defer.push(std::move(out.swapchain.storage));
	out.swapchain.storage.swapchain = vk::UniqueSwapchainKHR{vk_swapchain, *out.device};
	auto const images = out.device->getSwapchainImagesKHR(*out.swapchain.storage.swapchain);
	out.swapchain.storage.images.clear();
	out.swapchain.storage.views.clear();
	for (auto const image : images) {
		out.swapchain.storage.views.insert(out.vma.get().make_image_view(image, out.swapchain.info.imageFormat));
		out.swapchain.storage.images.insert({image, *out.swapchain.storage.views.span().back(), out.swapchain.info.imageExtent});
	}

	logger::info("[Swapchain] extent: [{}x{}] | images: [{}] | colour space: [{}] | vsync: [{}]", create_info.imageExtent.width, create_info.imageExtent.height,
				 out.swapchain.storage.images.size(), levk::is_srgb(out.swapchain.info.imageFormat) ? "sRGB" : "linear",
				 vsync_status(out.swapchain.info.presentMode));
	return ret;
}

levk::ImageBarrier undef_to_colour(levk::ImageView const& image) {
	auto ret = levk::ImageBarrier{};
	ret.barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead;
	ret.barrier.oldLayout = vk::ImageLayout::eUndefined;
	ret.barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
	ret.barrier.image = image.image;
	ret.barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
	ret.stages = {vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput};
	return ret;
}

levk::ImageBarrier undef_to_depth(levk::ImageView const& image) {
	auto ret = levk::ImageBarrier{};
	ret.barrier.srcAccessMask = ret.barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentRead;
	ret.barrier.oldLayout = vk::ImageLayout::eUndefined;
	ret.barrier.newLayout = vk::ImageLayout::eDepthAttachmentOptimal;
	ret.barrier.image = image.image;
	ret.barrier.subresourceRange = {vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1};
	ret.stages.first = ret.stages.second = vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests;
	return ret;
}

levk::ImageBarrier colour_to_present(levk::ImageView const& image) {
	auto ret = levk::ImageBarrier{};
	ret.barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead;
	ret.barrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
	ret.barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
	ret.barrier.image = image.image;
	ret.barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
	ret.stages = {vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe};
	return ret;
}
} // namespace

namespace levk {
void Vma::Deleter::operator()(Vma const& vma) const {
	if (vma.allocator) { vmaDestroyAllocator(vma.allocator); }
}

void Vma::Deleter::operator()(Buffer const& buffer) const {
	auto const& allocation = buffer.allocation;
	if (buffer.buffer && allocation.allocation) {
		if (buffer.ptr) { vmaUnmapMemory(allocation.vma.allocator, allocation.allocation); }
		vmaDestroyBuffer(allocation.vma.allocator, buffer.buffer, allocation.allocation);
	}
}

void Vma::Deleter::operator()(Image const& image) const {
	auto const& allocation = image.allocation;
	if (image.image && allocation.allocation) { vmaDestroyImage(allocation.vma.allocator, image.image, allocation.allocation); }
}

UniqueBuffer Vma::make_buffer(vk::BufferUsageFlags const usage, vk::DeviceSize const size, bool host_visible) const {
	auto vaci = VmaAllocationCreateInfo{};
	vaci.usage = VMA_MEMORY_USAGE_AUTO;
	if (host_visible) { vaci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT; }
	auto const bci = vk::BufferCreateInfo{{}, size, usage};
	auto vbci = static_cast<VkBufferCreateInfo>(bci);
	auto buffer = VkBuffer{};
	auto ret = Buffer{};
	if (vmaCreateBuffer(allocator, &vbci, &vaci, &buffer, &ret.allocation.allocation, {}) != VK_SUCCESS) { throw Error{"Failed to allocate Vulkan Buffer"}; }
	ret.buffer = buffer;
	ret.allocation.vma = *this;
	ret.size = bci.size;
	if (host_visible) { vmaMapMemory(allocator, ret.allocation.allocation, &ret.ptr); }
	return UniqueBuffer{std::move(ret)};
}

UniqueImage Vma::make_image(ImageCreateInfo const& info, vk::Extent2D const extent, vk::ImageViewType type) const {
	if (extent.width == 0 || extent.height == 0) { throw Error{"Attempt to allocate 0-sized image"}; }
	auto vaci = VmaAllocationCreateInfo{};
	vaci.usage = VMA_MEMORY_USAGE_AUTO;
	auto ici = vk::ImageCreateInfo{};
	ici.usage = info.usage;
	ici.imageType = vk::ImageType::e2D;
	if (type == vk::ImageViewType::eCube) { ici.flags |= vk::ImageCreateFlagBits::eCubeCompatible; }
	ici.tiling = info.tiling;
	ici.arrayLayers = info.array_layers;
	ici.mipLevels = info.mip_levels;
	ici.extent = vk::Extent3D{extent, 1};
	ici.format = info.format;
	ici.samples = info.samples;
	auto const vici = static_cast<VkImageCreateInfo>(ici);
	auto image = VkImage{};
	auto ret = Image{};
	if (vmaCreateImage(allocator, &vici, &vaci, &image, &ret.allocation.allocation, {}) != VK_SUCCESS) { throw Error{"Failed to allocate Vulkan Image"}; }
	ret.view = make_image_view(image, info.format, {info.aspect, 0, info.mip_levels, 0, info.array_layers}, type);
	ret.image = image;
	ret.allocation.vma = *this;
	ret.extent = extent;
	return UniqueImage{std::move(ret)};
}

vk::UniqueImageView Vma::make_image_view(vk::Image const image, vk::Format const format, vk::ImageSubresourceRange isr, vk::ImageViewType type) const {
	vk::ImageViewCreateInfo info;
	info.viewType = type;
	info.format = format;
	info.components.r = info.components.g = info.components.b = info.components.a = vk::ComponentSwizzle::eIdentity;
	info.subresourceRange = isr;
	info.image = image;
	return device.createImageViewUnique(info);
}

auto Cmd::Allocator::make(vk::Device device, std::uint32_t queue_family, vk::CommandPoolCreateFlags flags) -> Allocator {
	auto cpci = vk::CommandPoolCreateInfo{flags, queue_family};
	auto ret = Allocator{};
	ret.pool = device.createCommandPoolUnique(cpci);
	ret.device = device;
	return ret;
}

vk::Result Cmd::Allocator::allocate(std::span<vk::CommandBuffer> out, bool secondary) const {
	auto const level = secondary ? vk::CommandBufferLevel::eSecondary : vk::CommandBufferLevel::ePrimary;
	auto cbai = vk::CommandBufferAllocateInfo{*pool, level, static_cast<std::uint32_t>(out.size())};
	return device.allocateCommandBuffers(&cbai, out.data());
}

vk::CommandBuffer Cmd::Allocator::allocate(bool secondary) const {
	auto ret = vk::CommandBuffer{};
	allocate({&ret, 1}, secondary);
	return ret;
}

Cmd::Cmd(VulkanDevice const& device, vk::PipelineStageFlags wait)
	: m_device(device), m_allocator(Allocator::make(*device.device, device.queue_family)), m_wait(wait) {
	cb = m_allocator.allocate(false);
	cb.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
}

Cmd::~Cmd() {
	cb.end();
	m_fence = m_device.device->createFenceUnique({});
	auto si = vk::SubmitInfo{};
	si.pWaitDstStageMask = &m_wait;
	si.commandBufferCount = 1u;
	si.pCommandBuffers = &cb;
	{
		auto lock = std::scoped_lock{m_device.shared->mutex};
		if (m_device.queue.submit(1u, &si, *m_fence) != vk::Result::eSuccess) { return; }
	}
	m_device.wait(*m_fence);
}

void RenderTarget::to_draw(vk::CommandBuffer const cb) {
	if (colour.view) { undef_to_colour(colour).transition(cb); }
	if (resolve.view) { undef_to_colour(resolve).transition(cb); }
	if (depth.view) { undef_to_depth(depth).transition(cb); }
}

void RenderTarget::to_present(vk::CommandBuffer const cb) {
	if (resolve.view) {
		colour_to_present(resolve).transition(cb);
	} else {
		colour_to_present(colour).transition(cb);
	}
}

RenderFrame RenderFrame::make(VulkanDevice const& device) {
	static constexpr auto flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
	auto ret = RenderFrame{
		.sync =
			Sync{
				.draw = device.device->createSemaphoreUnique({}),
				.present = device.device->createSemaphoreUnique({}),
				.drawn = device.device->createFenceUnique({vk::FenceCreateFlagBits::eSignaled}),
			},
		.cmd_alloc = Cmd::Allocator::make(*device.device, device.queue_family, flags),
	};
	ret.primary = ret.cmd_alloc.allocate(false);
	ret.secondary = ret.cmd_alloc.allocate(true);
	return ret;
}

void DearImGui::new_frame() {
	if (state == State::eEndFrame) { end_frame(); }
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	state = State::eEndFrame;
}

void DearImGui::end_frame() {
	// ImGui::Render calls ImGui::EndFrame
	if (state == State::eNewFrame) { new_frame(); }
	ImGui::Render();
	state = State::eNewFrame;
}

void DearImGui::render(vk::CommandBuffer cb) {
	if (auto* data = ImGui::GetDrawData()) { ImGui_ImplVulkan_RenderDrawData(data, cb); }
}

RenderTarget VulkanDevice::make_render_target(ImageView c) {
	auto ret = RenderTarget{};
	if (render_pass.colour.format != vk::Format::eUndefined) {
		if (render_pass.colour.image.get().extent != c.extent) {
			auto const ici = ImageCreateInfo{
				.format = render_pass.colour.format,
				.usage = vk::ImageUsageFlagBits::eColorAttachment,
				.aspect = vk::ImageAspectFlagBits::eColor,
				.samples = from(info.current_aa),
			};
			shared->defer.push(std::move(render_pass.colour.image));
			render_pass.colour.image = vma.get().make_image(ici, c.extent);
		}
		ret.colour = render_pass.colour.image.get().image_view();
		ret.resolve = c;
	} else {
		ret.colour = c;
	}
	if (render_pass.depth.format != vk::Format::eUndefined) {
		if (render_pass.depth.image.get().extent != c.extent) {
			auto const ici = ImageCreateInfo{
				.format = render_pass.depth.format,
				.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
				.aspect = vk::ImageAspectFlagBits::eDepth,
				.samples = from(info.current_aa),
			};
			shared->defer.push(std::move(render_pass.depth.image));
			render_pass.depth.image = vma.get().make_image(ici, c.extent);
		}
		ret.depth = render_pass.depth.image.get().image_view();
	}
	ret.extent = c.extent;
	return ret;
}
} // namespace levk

void levk::gfx_create_device(VulkanDevice& out, GraphicsDeviceCreateInfo const& create_info) {
	auto const extensions = create_info.window.vulkan_extensions();
	out.instance = make_instance({extensions.begin(), extensions.end()}, create_info, out.info);
	if (create_info.validation) { out.debug = make_debug_messenger(*out.instance); }
	auto surface = VulkanSurface{};
	auto instance = VulkanSurface::Instance{};
	instance.instance = *out.instance;
	create_info.window.make_surface(surface, instance);
	out.surface = std::move(surface.surface);
	out.gpu = select_gpu(*out.instance, *out.surface);
	auto layers = FlexArray<char const*, 2>{};
	if (create_info.validation) { layers.insert(validation_layer_v.data()); }
	out.device = make_device(layers.span(), out.gpu);
	out.queue = out.device->getQueue(out.gpu.queue_family, 0);
	out.shared = std::make_unique<levk::VulkanDevice::Shared>();

	out.vma = make_vma(*out.instance, out.gpu.device, *out.device);

	out.swapchain.formats = make_swapchain_formats(out.gpu.device.getSurfaceFormatsKHR(*out.surface));
	auto const supported_modes = out.gpu.device.getSurfacePresentModesKHR(*out.surface);
	out.swapchain.modes = {supported_modes.begin(), supported_modes.end()};
	out.swapchain.info = make_swci(out.queue_family, *out.surface, out.swapchain.formats, create_info.swapchain, vk::PresentModeKHR::eFifo);
	out.info.swapchain = is_srgb(out.swapchain.info.imageFormat) ? ColourSpace::eSrgb : ColourSpace::eLinear;
	out.info.supported_vsync = make_vsync(out.gpu.device, *out.surface);
	out.info.current_vsync = from(out.swapchain.info.presentMode);
	out.info.supported_aa = make_aa(out.gpu.properties);
	out.info.current_aa = get_samples(out.info.supported_aa, create_info.anti_aliasing);

	auto const depth = depth_format(out.gpu.device);
	out.render_pass.render_pass = make_render_pass(*out.device, from(out.info.current_aa), out.swapchain.info.imageFormat, depth);
	out.render_pass.depth.format = depth;
	if (out.info.current_aa > AntiAliasing::e1x) { out.render_pass.colour.format = out.swapchain.info.imageFormat; }
	for (auto& frame : out.render_frames.t) { frame = RenderFrame::make(out); }
	out.dear_imgui = make_imgui(create_info.window, out);
	out.dear_imgui.new_frame();
}

void levk::gfx_destroy_device(VulkanDevice& out) {
	out.device->waitIdle();
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

bool levk::gfx_set_vsync(VulkanDevice& out, Vsync::Type vsync) { return make_swapchain(out, {}, from(vsync)) == vk::Result::eSuccess; }

void levk::gfx_render(VulkanDevice& out_device, GraphicsRenderer& out_renderer, glm::uvec2 const framebuffer_extent) {
	struct ImGuiNewFrame {
		DearImGui& out;
		~ImGuiNewFrame() { out.new_frame(); }
	};

	auto const imgui_new_frame = ImGuiNewFrame{out_device.dear_imgui};
	if (framebuffer_extent.x == 0 || framebuffer_extent.y == 0) { return; }

	auto& frame = out_device.render_frames.get();

	struct {
		ImageView image;
		std::uint32_t index;
	} acquired{};
	auto recreate = [&out_device, framebuffer_extent] {
		if (make_swapchain(out_device, framebuffer_extent, {}) == vk::Result::eSuccess) {
			out_device.swapchain.storage.extent = framebuffer_extent;
			return true;
		}
		return false;
	};

	if (framebuffer_extent != out_device.swapchain.storage.extent && !recreate()) { return; }

	auto result = vk::Result::eErrorOutOfDateKHR;
	{
		auto lock = std::scoped_lock{out_device.shared->mutex};
		// acquire swapchain image
		result = out_device.device->acquireNextImageKHR(*out_device.swapchain.storage.swapchain, std::numeric_limits<std::uint64_t>::max(), *frame.sync.draw,
														{}, &acquired.index);
	}
	switch (result) {
	case vk::Result::eErrorOutOfDateKHR:
	case vk::Result::eSuboptimalKHR: {
		if (!recreate()) { return; }
		break;
	}
	case vk::Result::eSuccess: break;
	default: return;
	}
	auto const images = out_device.swapchain.storage.images.span();
	assert(acquired.index < images.size());
	acquired.image = images[acquired.index];
	out_device.swapchain.storage.extent = framebuffer_extent;

	if (!acquired.image.image) { return; }

	out_device.reset(*frame.sync.drawn);
	out_device.shared->defer.next();

	// refresh render target
	auto render_target = out_device.make_render_target(acquired.image);
	auto const attachments = render_target.attachments();

	// refresh framebuffer
	{
		auto fbci = vk::FramebufferCreateInfo{};
		fbci.renderPass = *out_device.render_pass.render_pass;
		auto const span = attachments.span();
		fbci.attachmentCount = static_cast<std::uint32_t>(span.size());
		fbci.pAttachments = span.data();
		fbci.width = render_target.colour.extent.width;
		fbci.height = render_target.colour.extent.height;
		fbci.layers = 1;
		frame.framebuffer = out_device.device->createFramebufferUnique(fbci);
	}
	auto const framebuffer = Framebuffer{attachments, *frame.framebuffer, frame.primary, frame.secondary};

	// begin recording commands
	auto const cbii = vk::CommandBufferInheritanceInfo{*out_device.render_pass.render_pass, 0, framebuffer.framebuffer};
	frame.secondary.begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue, &cbii});

	auto vcb = VulkanRenderContext{};
	vcb.cb = frame.secondary;
	out_renderer.render(vcb);
	out_device.dear_imgui.end_frame();

	// perform the actual Dear ImGui render
	out_device.dear_imgui.render(frame.secondary);
	// end recording
	frame.secondary.end();

	// prepare render pass
	frame.primary.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
	// transition render target to be ready to draw to
	render_target.to_draw(frame.primary);

	auto const clear_colour = vk::ClearColorValue{std::array{0.0f, 0.0f, 0.0f, 0.0f}};
	// execute render pass
	vk::ClearValue const clear_values[] = {clear_colour, vk::ClearDepthStencilValue{1.0f, 0u}};
	auto rpbi = vk::RenderPassBeginInfo{*out_device.render_pass.render_pass, *frame.framebuffer, vk::Rect2D{{}, {framebuffer_extent.x, framebuffer_extent.y}},
										clear_values};
	frame.primary.beginRenderPass(rpbi, vk::SubpassContents::eSecondaryCommandBuffers);
	frame.primary.executeCommands(1u, &frame.secondary);
	frame.primary.endRenderPass();

	// transition render target to be ready to be presented
	render_target.to_present(frame.primary);
	// end render pass
	frame.primary.end();

	static constexpr vk::PipelineStageFlags submit_wait = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	auto submit_info = vk::SubmitInfo{*frame.sync.draw, submit_wait, frame.primary, *frame.sync.present};

	auto present_info = vk::PresentInfoKHR{};
	present_info.swapchainCount = 1u;
	present_info.pSwapchains = &*out_device.swapchain.storage.swapchain;
	present_info.waitSemaphoreCount = 1u;
	present_info.pWaitSemaphores = &*frame.sync.present;
	present_info.pImageIndices = &acquired.index;

	{
		auto lock = std::scoped_lock{out_device.shared->mutex};
		// submit commands
		out_device.queue.submit(submit_info, *frame.sync.drawn);
		// present image
		result = out_device.queue.presentKHR(&present_info);
	}

	if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR) { recreate(); }

	// rotate everything
	// out_device.pipes.rotate();
	out_device.render_frames.rotate();
}
