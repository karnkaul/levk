#pragma once
#include <vk_mem_alloc.h>
#include <levk/graphics/image.hpp>
#include <levk/graphics/shader_code.hpp>
#include <levk/graphics/texture_sampler.hpp>
#include <levk/uri.hpp>
#include <levk/util/defer_queue.hpp>
#include <levk/util/flex_array.hpp>
#include <levk/util/ptr.hpp>
#include <levk/util/unique.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_hash.hpp>
#include <optional>
#include <unordered_map>

namespace levk::vulkan {
struct PipelineStorage;

inline constexpr vk::Format srgb_formats_v[] = {vk::Format::eR8G8B8A8Srgb, vk::Format::eB8G8R8A8Srgb, vk::Format::eA8B8G8R8SrgbPack32};
inline constexpr vk::Format linear_formats_v[] = {vk::Format::eR8G8B8A8Unorm, vk::Format::eB8G8R8A8Unorm};

template <std::integral Type = std::size_t>
constexpr auto buffering_v = Type{1};

struct Index {
	std::size_t value{};
	constexpr operator std::size_t() const { return value; }
	constexpr void next() { value = (value + 1) % buffering_v<>; }
};

template <typename Type>
using Buffered = std::array<Type, buffering_v<>>;

bool is_srgb(vk::Format format);

template <typename Type>
struct DeferDeleter {
	Ptr<DeferQueue> queue{};

	void operator()(Type t) const {
		if (queue) { queue->push(std::move(t)); }
	}
};

template <typename Type>
class Defer {
	using Deleter = DeferDeleter<Type>;

  public:
	Defer() = default;
	Defer(DeferQueue& queue, Type t = {}) : m_t(std::move(t), Deleter{&queue}) {}

	void swap(Type t) { m_t = {std::move(t), m_t.get_deleter()}; }

	Type& get() { return m_t.get(); }
	Type const& get() const { return m_t.get(); }

  private:
	Unique<Type, Deleter> m_t;
};

struct ShaderHash {
	std::size_t value{};

	auto operator<=>(ShaderHash const&) const = default;

	explicit constexpr operator bool() const { return value > 0u; }

	struct Hasher {
		std::size_t operator()(ShaderHash sh) const { return sh.value; }
	};
};

template <typename Type>
struct VertFrag {
	Type vert{};
	Type frag{};
};

struct BufferView {
	vk::Buffer buffer{};
	vk::DeviceSize size{};
	vk::DeviceSize offset{};
	std::uint32_t count{1};
};

struct ImageView {
	vk::Image image{};
	vk::ImageView view{};
	vk::Extent2D extent{};
	vk::Format format{};
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

	struct Blit {
		ImageView const& image;
		std::uint32_t mip_levels{1u};
	};

	struct Copy {
		Image const& image;
		vk::ImageLayout layout{};
		glm::ivec2 offset{};
	};

	static constexpr auto isr_v = vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

	static Unique<Vma, Deleter> make(vk::Instance instance, vk::PhysicalDevice gpu, vk::Device device);

	vk::Device device{};
	VmaAllocator allocator{};

	Unique<Buffer, Deleter> make_buffer(vk::BufferUsageFlags usage, vk::DeviceSize size, bool host_visible) const;
	Unique<Image, Deleter> make_image(ImageCreateInfo const& info, vk::Extent2D extent, vk::ImageViewType type = vk::ImageViewType::e2D) const;
	vk::UniqueImageView make_image_view(vk::Image const image, vk::Format const format, vk::ImageSubresourceRange isr = isr_v,
										vk::ImageViewType type = vk::ImageViewType::e2D) const;

	void copy_image(vk::CommandBuffer cb, Copy const& src, Copy const& dst, vk::Extent2D const extent) const;
	[[nodiscard]] Unique<Buffer, Deleter> copy_to_image(vk::CommandBuffer cb, Image const& out, std::span<levk::Image::View const> images,
														glm::ivec2 const offset = {}) const;
	[[nodiscard]] Unique<Buffer, Deleter> write_images(vk::CommandBuffer cb, Image const& out, std::span<ImageWrite const> writes) const;

	void* map_memory(Buffer& out) const;
	void unmap_memory(Buffer& out) const;

	template <typename F>
	void with_mapped(Buffer& out, F func) const {
		func(map_memory(out));
		unmap_memory(out);
	}

	void full_blit(vk::CommandBuffer cb, Blit src, Blit dst, vk::Filter filter = vk::Filter::eLinear) const;
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
	void* mapped{};

	bool operator==(Buffer const& rhs) const { return allocation.allocation == rhs.allocation.allocation; }

	BufferView view() const { return {buffer, size}; }
};

struct Vma::Image {
	Allocation allocation{};
	vk::Image image{};
	vk::UniqueImageView view{};
	vk::Format format{};

	vk::Extent2D extent{};
	vk::ImageViewType type{};
	std::uint32_t mip_levels{};
	std::uint32_t array_layers{};

	ImageView image_view() const { return {image, *view, extent, format}; }

	bool operator==(Image const& rhs) const { return allocation.allocation == rhs.allocation.allocation; }
};

using UniqueBuffer = Unique<Vma::Buffer, Vma::Deleter>;
using UniqueImage = Unique<Vma::Image, Vma::Deleter>;

struct Gpu {
	vk::PhysicalDevice device{};
	vk::PhysicalDeviceProperties properties{};
	std::uint32_t queue_family{};

	explicit operator bool() const { return !!device; }
};

struct SetLayout {
	static constexpr std::size_t max_sets_v{8};
	static constexpr std::size_t max_bindings_v{8};

	FlexArray<vk::DescriptorSetLayoutBinding, max_bindings_v> bindings{};
	std::uint32_t set{};
};

struct ShaderLayout {
	std::span<SetLayout const> set_layouts{};
	ShaderHash hash{};
};

struct PipelineFormat {
	vk::Format colour{};
	vk::Format depth{};
	vk::SampleCountFlagBits samples{vk::SampleCountFlagBits::e1};

	bool operator==(PipelineFormat const&) const = default;

	struct Hasher {
		std::size_t operator()(PipelineFormat const& f) const;
	};
};

struct Queue {
	vk::Queue queue{};
	std::uint32_t family{};
	std::uint32_t index{};

	std::mutex mutex{};

	static void make(Queue& out, vk::Device device, std::uint32_t family, std::uint32_t index = 0);

	template <typename F>
	decltype(auto) with(F f) {
		auto lock = std::scoped_lock{mutex};
		return f(queue);
	}
};

struct CommandAllocator {
	static constexpr vk::CommandPoolCreateFlags flags_v = vk::CommandPoolCreateFlagBits::eTransient;

	vk::Device device{};
	vk::UniqueCommandPool pool{};

	static CommandAllocator make(vk::Device device, std::uint32_t queue_family, vk::CommandPoolCreateFlags flags = flags_v) {
		auto cpci = vk::CommandPoolCreateInfo{flags, queue_family};
		auto ret = CommandAllocator{};
		ret.pool = device.createCommandPoolUnique(cpci);
		ret.device = device;
		return ret;
	}

	vk::Result allocate(std::span<vk::CommandBuffer> out) const {
		auto cbai = vk::CommandBufferAllocateInfo{*pool, vk::CommandBufferLevel::ePrimary, static_cast<std::uint32_t>(out.size())};
		return device.allocateCommandBuffers(&cbai, out.data());
	}

	vk::CommandBuffer allocate() const {
		auto ret = vk::CommandBuffer{};
		allocate({&ret, 1});
		return ret;
	}
};

struct SetAllocator {
	static vk::UniqueDescriptorPool make_descriptor_pool(vk::Device device, std::uint32_t max_sets = 32);

	static SetAllocator make(vk::Device device) {
		auto ret = SetAllocator{};
		ret.device = device;
		ret.pools.push_back(make_descriptor_pool(device));
		return ret;
	}

	vk::Device device{};
	std::vector<vk::UniqueDescriptorPool> pools{};

	vk::DescriptorSet allocate(vk::DescriptorSetLayout layout);
	void reset_all();

	vk::DescriptorSet try_allocate(vk::DescriptorSetLayout layout);
};

struct VertexInput {
	static constexpr std::size_t max_v{16};

	struct View {
		std::span<vk::VertexInputAttributeDescription const> attributes{};
		std::span<vk::VertexInputBindingDescription const> bindings{};
		std::size_t hash{};
	};

	FlexArray<vk::VertexInputAttributeDescription, max_v> attributes{};
	FlexArray<vk::VertexInputBindingDescription, max_v> bindings{};
	mutable std::size_t hash{};

	std::size_t make_hash() const;

	View view() const {
		if (attributes.empty()) { return {}; }
		if (hash == 0) { make_hash(); }
		return {attributes.span(), bindings.span(), hash};
	}

	operator View() const { return view(); }
};

struct BufferWriter {
	Vma::Buffer& out;
	std::size_t offset{};

	template <typename T>
	std::size_t operator()(std::span<T> data) {
		auto ret = offset;
		std::memcpy(static_cast<std::byte*>(out.mapped) + offset, data.data(), data.size_bytes());
		offset += data.size_bytes();
		return ret;
	}
};

struct ScratchBufferAllocator {
	Vma vma{};
	std::vector<UniqueBuffer> buffers{};

	Vma::Buffer& allocate(vk::DeviceSize size, vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eUniformBuffer) {
		assert(vma.allocator);
		return buffers.emplace_back(vma.make_buffer(usage, size, true)).get();
	}

	void clear() { buffers.clear(); }
};

struct SamplerStorage {
	struct Hasher {
		std::size_t operator()(TextureSampler const& sampler) const;
	};

	std::unordered_map<TextureSampler, vk::UniqueSampler, Hasher> map{};
	float anisotropy{};

	vk::Sampler get(vk::Device device, TextureSampler const& sampler);
};

struct GlobalLayout {
	struct Storage {
		vk::UniqueDescriptorSetLayout global_set_layout{};
		vk::UniquePipelineLayout global_pipeline_layout{};
	};

	vk::DescriptorSetLayout global_set_layout{};
	vk::PipelineLayout global_pipeline_layout{};
};

struct DeviceView {
	vk::Instance instance{};
	vk::SurfaceKHR surface{};
	vk::Device device{};
	Gpu gpu{};

	Vma vma{};
	GlobalLayout global_layout{};
	Ptr<Queue> queue{};
	Ptr<DeferQueue> defer{};
	Ptr<SetAllocator> set_allocator{};
	Ptr<ScratchBufferAllocator> scratch_buffer_allocator{};
	Ptr<PipelineStorage> pipeline_storage{};
	Ptr<SamplerStorage> sampler_storage{};
	Ptr<Index const> buffered_index{};

	CommandAllocator make_command_allocator(vk::CommandPoolCreateFlags flags = CommandAllocator::flags_v) const {
		return CommandAllocator::make(device, gpu.queue_family, flags);
	}

	vk::Result wait(vk::ArrayProxy<vk::Fence> const& fences) const { return device.waitForFences(fences, true, std::numeric_limits<std::uint64_t>::max()); }

	bool can_mip(vk::Format const format) const {
		static constexpr vk::FormatFeatureFlags flags_v{vk::FormatFeatureFlagBits::eBlitDst | vk::FormatFeatureFlagBits::eBlitSrc};
		auto const fsrc = gpu.device.getFormatProperties(format);
		return (fsrc.optimalTilingFeatures & flags_v) != vk::FormatFeatureFlags{};
	}

	static std::uint32_t compute_mip_levels(vk::Extent2D extent);
};

struct GeometryOffsets {
	std::size_t positions{};
	std::size_t rgbs{};
	std::size_t normals{};
	std::size_t uvs{};
	std::size_t joints{};
	std::size_t weights{};
	std::size_t indices{};
};

struct GeometryLayout {
	VertexInput vertex_input{};
	GeometryOffsets offsets{};
	std::optional<std::uint32_t> instance_binding{};
	std::optional<std::uint32_t> joints_binding{};
	std::optional<std::uint32_t> joints_set{};
	std::uint32_t vertices{};
	std::uint32_t indices{};
	std::uint32_t joints{};
};

struct DeviceBuffer {
	Defer<UniqueBuffer> buffer{};
	vk::BufferUsageFlags usage{};
	DeviceView device{};
	std::uint32_t count{};

	static DeviceBuffer make(DeviceView device, vk::BufferUsageFlags usage);

	void write(void const* data, std::size_t size, std::size_t count = 1u);
	BufferView view() const;
};

struct HostBuffer {
	std::vector<std::byte> bytes{};
	Defer<Buffered<UniqueBuffer>> buffers{};
	vk::BufferUsageFlags usage{};
	DeviceView device{};
	std::uint32_t count{};

	static HostBuffer make(DeviceView device, vk::BufferUsageFlags usage);

	void write(void const* data, std::size_t size, std::size_t count = 1u);

	BufferView view();
};
} // namespace levk::vulkan
