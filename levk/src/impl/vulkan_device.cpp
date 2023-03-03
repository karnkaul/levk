#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <vk_mem_alloc.h>
#include <glm/gtc/color_space.hpp>
#include <glm/mat4x4.hpp>
#include <levk/asset/asset_providers.hpp>
#include <levk/geometry.hpp>
#include <levk/graphics_device.hpp>
#include <levk/impl/defer_queue.hpp>
#include <levk/impl/desktop_window.hpp>
#include <levk/impl/vulkan_device.hpp>
#include <levk/impl/vulkan_surface.hpp>
#include <levk/material.hpp>
#include <levk/renderer.hpp>
#include <levk/skinned_mesh.hpp>
#include <levk/static_mesh.hpp>
#include <levk/surface.hpp>
#include <levk/texture.hpp>
#include <levk/util/dyn_array.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/error.hpp>
#include <levk/util/fixed_string.hpp>
#include <levk/util/flex_array.hpp>
#include <levk/util/hash_combine.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/ptr.hpp>
#include <levk/util/unique.hpp>
#include <levk/util/zip_ranges.hpp>
#include <levk/window.hpp>
#include <spirv_glsl.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_hash.hpp>
#include <algorithm>
#include <atomic>
#include <map>
#include <numeric>
#include <optional>
#include <unordered_set>

namespace levk {
namespace {
template <typename T, typename U = T>
using Pair = std::pair<T, U>;

constexpr vk::Format srgb_formats_v[] = {vk::Format::eR8G8B8A8Srgb, vk::Format::eB8G8R8A8Srgb, vk::Format::eA8B8G8R8SrgbPack32};
constexpr vk::Format linear_formats_v[] = {vk::Format::eR8G8B8A8Unorm, vk::Format::eB8G8R8A8Unorm};
constexpr std::size_t buffering_v{2};

constexpr bool is_linear(vk::Format format) {
	return std::find(std::begin(linear_formats_v), std::end(linear_formats_v), format) != std::end(linear_formats_v);
}

constexpr bool is_srgb(vk::Format format) { return std::find(std::begin(srgb_formats_v), std::end(srgb_formats_v), format) != std::end(srgb_formats_v); }

constexpr vk::SamplerAddressMode from(TextureSampler::Wrap const wrap) {
	switch (wrap) {
	case TextureSampler::Wrap::eClampBorder: return vk::SamplerAddressMode::eClampToBorder;
	case TextureSampler::Wrap::eClampEdge: return vk::SamplerAddressMode::eClampToEdge;
	default: return vk::SamplerAddressMode::eRepeat;
	}
}

constexpr vk::Filter from(TextureSampler::Filter const filter) {
	switch (filter) {
	case TextureSampler::Filter::eNearest: return vk::Filter::eNearest;
	default: return vk::Filter::eLinear;
	}
}

constexpr vk::Extent2D scale_extent(vk::Extent2D const extent, float scale) {
	glm::vec2 vec2 = glm::uvec2{extent.width, extent.height};
	vec2 *= scale;
	auto ret = glm::uvec2{vec2};
	return {ret.x, ret.y};
}

std::uint32_t compute_mip_levels(vk::Extent2D extent) { return static_cast<std::uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1u; }

template <typename Type>
struct DeferDeleter {
	Ptr<DeferQueue> queue{};

	void operator()(Type t) const { queue->push(std::move(t)); }
};

template <typename Type>
class Defer {
  public:
	Defer() = default;
	Defer(DeferQueue& queue, Type t = {}) : m_t(std::move(t), Deleter{&queue}) {}

	void swap(Type t) { m_t = {std::move(t), m_t.get_deleter()}; }

	Type& get() { return m_t.get(); }
	Type const& get() const { return m_t.get(); }

  private:
	struct Deleter {
		Ptr<DeferQueue> queue{};

		void operator()(Type t) const {
			if (queue) { queue->push(std::move(t)); }
		}
	};

	Unique<Type, Deleter> m_t{};
};

vk::Result wait(vk::Device device, vk::ArrayProxy<vk::Fence> const& fences) {
	return device.waitForFences(fences, true, std::numeric_limits<std::uint64_t>::max());
}

void reset(vk::Device device, vk::Fence fence) {
	wait(device, fence);
	device.resetFences(fence);
}

[[maybe_unused]] void full_barrier(vk::CommandBuffer cb) {
	static constexpr auto all_stages = vk::PipelineStageFlagBits::eAllGraphics;
	static constexpr auto all_access = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
	cb.pipelineBarrier(all_stages, all_stages, {}, vk::MemoryBarrier{all_access, all_access}, {}, {});
}

template <std::size_t Buffering = buffering_v>
struct Index {
	std::size_t value{};
	explicit constexpr operator std::size_t() const { return value; }
	constexpr void next() { value = (value + 1) % Buffering; }
};

template <typename Type, std::size_t Size>
struct Buffered {
	Type t[Size]{};

	constexpr Type& get(Index<Size> index) { return t[index.value]; }
	constexpr Type const& get(Index<Size> index) const { return t[index.value]; }
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

	bool operator==(Buffer const& rhs) const { return allocation.allocation == rhs.allocation.allocation; }

	BufferView view() const { return {buffer, size}; }
};

struct Vma::Image {
	Allocation allocation{};
	vk::Image image{};
	vk::UniqueImageView view{};

	vk::Extent2D extent{};
	vk::ImageViewType type{};
	std::uint32_t mip_levels{};
	std::uint32_t array_layers{};

	ImageView image_view() const { return {image, *view, extent}; }

	bool operator==(Image const& rhs) const { return allocation.allocation == rhs.allocation.allocation; }
};

struct CopyImage {
	Vma::Image const& image;
	vk::ImageLayout layout{};
	glm::ivec2 offset{};
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

struct CmdAllocator {
	static constexpr vk::CommandPoolCreateFlags flags_v = vk::CommandPoolCreateFlagBits::eTransient;

	vk::Device device{};
	vk::UniqueCommandPool pool{};

	static CmdAllocator make(vk::Device device, std::uint32_t queue_family, vk::CommandPoolCreateFlags flags = flags_v) {
		auto cpci = vk::CommandPoolCreateInfo{flags, queue_family};
		auto ret = CmdAllocator{};
		ret.pool = device.createCommandPoolUnique(cpci);
		ret.device = device;
		return ret;
	}

	vk::Result allocate(std::span<vk::CommandBuffer> out, bool secondary = false) const {
		auto const level = secondary ? vk::CommandBufferLevel::eSecondary : vk::CommandBufferLevel::ePrimary;
		auto cbai = vk::CommandBufferAllocateInfo{*pool, level, static_cast<std::uint32_t>(out.size())};
		return device.allocateCommandBuffers(&cbai, out.data());
	}

	vk::CommandBuffer allocate(bool secondary) const {
		auto ret = vk::CommandBuffer{};
		allocate({&ret, 1}, secondary);
		return ret;
	}
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

	ImageView const& output_image() const { return resolve.image ? resolve : colour; }
};

struct Framebuffer {
	vk::UniqueFramebuffer framebuffer{};
	RenderTarget render_target{};
};

struct RenderFrame {
	struct Sync {
		vk::UniqueSemaphore draw{};
		vk::UniqueSemaphore present{};
		vk::UniqueFence drawn{};
	};

	Sync sync{};
	CmdAllocator cmd_alloc{};
	Framebuffer framebuffer{};
	vk::CommandBuffer primary{};
	std::array<vk::CommandBuffer, 2> secondary{};

	static RenderFrame make(VulkanDevice const& device);
};

struct RenderCmd {
	vk::CommandBuffer cb{};
	vk::Extent2D extent{};
};

template <std::size_t Buffering>
using RenderFrames = Buffered<RenderFrame, Buffering>;

struct RenderPassView {
	vk::RenderPass render_pass{};
	vk::SampleCountFlagBits samples{};
};

template <std::size_t Buffering = buffering_v>
struct RenderPassStorage {
	using View = RenderPassView;

	RenderFrames<Buffering> frames{};
	vk::UniqueRenderPass render_pass{};
	vk::SampleCountFlagBits samples{};
	vk::Format colour{};
	vk::Format depth{};

	View view() const { return {*render_pass, samples}; }
};

template <std::size_t Buffering = buffering_v>
struct OffScreen {
	RenderPassStorage<Buffering> rp{};

	Defer<UniqueImage> output_image{};
	Defer<UniqueImage> msaa_image{};
	Defer<UniqueImage> depth_buffer{};

	static OffScreen make(VulkanDevice const& device, vk::SampleCountFlagBits samples, vk::Format depth, vk::Format colour = vk::Format::eR8G8B8A8Srgb);

	Framebuffer make_framebuffer(VulkanDevice const& device, vk::Extent2D extent, float scale);
	RenderPassView view() const { return rp.view(); }
};

template <std::size_t Buffering = buffering_v>
struct FsQuad {
	Buffered<Framebuffer, Buffering> framebuffer{};
	vk::UniqueRenderPass rp{};

	Framebuffer make_framebuffer(VulkanDevice const& device, ImageView const& output_image);
	RenderPassView view() const { return {*rp, vk::SampleCountFlagBits::e1}; }
};

struct DearImGui {
	enum class State { eNewFrame, eEndFrame };

	vk::UniqueDescriptorPool pool{};
	State state{};

	void new_frame();
	void end_frame();
	void render(vk::CommandBuffer cb);
};

struct SpirV {
	std::span<std::uint32_t const> code{};
	std::size_t hash{};

	static SpirV from(ShaderProvider& provider, Uri<ShaderCode> const& uri) {
		auto data = provider.load(uri);
		if (!data) { return {}; }
		return {data->spir_v.span(), data->hash};
	}

	explicit operator bool() const { return !code.empty() && hash > 0; }
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

	std::size_t make_hash() const {
		hash = std::size_t{};
		for (auto const& binding : bindings.span()) { hash_combine(hash, binding.binding, binding.stride, binding.inputRate); }
		for (auto const& attribute : attributes.span()) { hash_combine(hash, attribute.binding, attribute.format, attribute.location, attribute.offset); }
		return hash;
	}

	View view() const {
		if (attributes.empty()) { return {}; }
		if (hash == 0) { make_hash(); }
		return {attributes.span(), bindings.span(), hash};
	}

	operator View() const { return view(); }
};

struct SetLayout {
	FlexArray<vk::DescriptorSetLayoutBinding, max_bindings_v> bindings{};
	std::uint32_t set{};
};

class DescriptorSet {
  public:
	DescriptorSet(Vma const& vma, SetLayout const& layout, vk::DescriptorSet set, std::uint32_t number)
		: m_layout(layout), m_vma(vma), m_set(set), m_number(number) {}

	void update(std::uint32_t binding, ImageView image, vk::Sampler sampler, vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal) const {
		auto const type = get_type(binding, vk::DescriptorType::eCombinedImageSampler);
		update(vk::DescriptorImageInfo{sampler, image.view, layout}, binding, type);
	}

	void update(std::uint32_t binding, BufferView buffer) const {
		auto const type = get_type(binding, vk::DescriptorType::eUniformBuffer, vk::DescriptorType::eStorageBuffer);
		update(vk::DescriptorBufferInfo{buffer.buffer, buffer.offset, buffer.size}, binding, type);
	}

	void write(std::uint32_t binding, void const* data, std::size_t size) {
		static constexpr auto get_usage = [](vk::DescriptorType const type) {
			switch (type) {
			default:
			case vk::DescriptorType::eUniformBuffer: return vk::BufferUsageFlagBits::eUniformBuffer;
			case vk::DescriptorType::eStorageBuffer: return vk::BufferUsageFlagBits::eStorageBuffer;
			}
		};
		auto const type = get_type(binding, vk::DescriptorType::eUniformBuffer, vk::DescriptorType::eStorageBuffer);
		auto& buffer = m_buffers.emplace_back();
		buffer = m_vma.make_buffer(get_usage(type), size, true);
		std::memcpy(buffer.get().ptr, data, size);
		update(vk::DescriptorBufferInfo{buffer.get().buffer, {}, buffer.get().size}, binding, type);
	}

	vk::DescriptorSet set() const { return m_set; }
	void clear() { m_buffers.clear(); }

	std::uint32_t number() const { return m_number; }

  private:
	template <typename T>
	void update(T const& t, std::uint32_t binding, vk::DescriptorType type) const {
		auto wds = vk::WriteDescriptorSet{m_set, binding, 0, 1, type};
		if constexpr (std::same_as<T, vk::DescriptorBufferInfo>) {
			wds.pBufferInfo = &t;
		} else {
			wds.pImageInfo = &t;
		}
		m_vma.device.updateDescriptorSets(wds, {});
	}

	template <typename... T>
	vk::DescriptorType get_type(std::uint32_t binding, T... match) const noexcept(false) {
		if (binding >= m_layout.bindings.span().size()) { throw Error{fmt::format("DescriptorSet: Invalid binding: {}", binding)}; }
		auto const& bind = m_layout.bindings.span()[binding];
		if ((... && (bind.descriptorType != match))) { throw Error{"DescriptorSet: Invalid descriptor type"}; }
		return bind.descriptorType;
	}

	SetLayout m_layout{};
	Vma m_vma{};
	std::vector<UniqueBuffer> m_buffers{};
	vk::DescriptorSet m_set{};
	std::uint32_t m_number{};
};

class SetAllocator {
  public:
	class Pool;

  private:
	static vk::UniqueDescriptorPool make_descriptor_pool(vk::Device device, SetLayout const& layout, std::uint32_t max_sets = 32) {
		auto pool_sizes = std::vector<vk::DescriptorPoolSize>{};
		for (auto const& binding : layout.bindings.span()) {
			auto& pool_size = pool_sizes.emplace_back();
			pool_size.type = binding.descriptorType;
			pool_size.descriptorCount = binding.descriptorCount;
		}
		if (pool_sizes.empty()) { return {}; }
		return device.createDescriptorPoolUnique({{}, max_sets, pool_sizes});
	}

	SetAllocator(Vma const& vma, SetLayout const& layout, std::uint32_t number) : m_layout(layout), m_vma(vma), m_number(number) {
		auto const dslci = vk::DescriptorSetLayoutCreateInfo{{}, static_cast<std::uint32_t>(m_layout.bindings.span().size()), m_layout.bindings.span().data()};
		m_set_layout = m_vma.device.createDescriptorSetLayoutUnique(dslci);
		auto pool = make_descriptor_pool(m_vma.device, m_layout);
		if (!pool) {
			m_empty = true;
			return;
		}
		m_pools.push_back(std::move(pool));
	}

	DescriptorSet& acquire() {
		if (!m_vma.device || m_empty) { throw Error{"Attempt to allocate descriptor set from empty allocator"}; }
		if (m_index >= m_sets.size()) {
			if (!try_allocate()) {
				m_pools.push_back(make_descriptor_pool(m_vma.device, m_layout));
				if (!try_allocate()) { throw Error{"Failed to allocate descriptor set"}; }
			}
		}
		assert(m_index < m_sets.size());
		auto& ret = m_sets[m_index++];
		ret.clear();
		return ret;
	}

	void release_all() { m_index = 0; }

	vk::DescriptorSetLayout const& descriptor_set_layout() const { return *m_set_layout; }

	bool try_allocate() {
		static constexpr std::uint32_t count{8};
		vk::DescriptorSetLayout layouts[count]{};
		for (auto& layout : layouts) { layout = *m_set_layout; }
		auto& pool = m_pools.back();
		auto dsai = vk::DescriptorSetAllocateInfo{*pool, layouts};
		dsai.descriptorSetCount = count;
		vk::DescriptorSet sets[count]{};
		if (m_vma.device.allocateDescriptorSets(&dsai, sets) != vk::Result::eSuccess) { return false; }
		m_sets.reserve(m_sets.size() + std::size(sets));
		for (auto set : sets) { m_sets.emplace_back(m_vma, m_layout, set, m_number); }
		return true;
	}

	SetLayout m_layout{};
	Vma m_vma{};
	vk::UniqueDescriptorSetLayout m_set_layout{};
	std::vector<vk::UniqueDescriptorPool> m_pools{};
	std::vector<DescriptorSet> m_sets{};
	std::size_t m_index{};
	std::uint32_t m_number{};
	bool m_empty{};
};

class SetAllocator::Pool {
  public:
	Pool() = default;
	Pool(Vma const& vma, std::span<SetLayout const> layouts) : device(vma.device) {
		auto number = std::uint32_t{};
		for (auto const& layout : layouts) { m_sets.push_back({vma, layout, number++}); }
	}

	FlexArray<vk::DescriptorSetLayout, max_sets_v> descriptor_set_layouts() const {
		auto ret = FlexArray<vk::DescriptorSetLayout, max_sets_v>{};
		for (auto const& set : m_sets) { ret.insert(set.descriptor_set_layout()); }
		return ret;
	}

	DescriptorSet& next_set(std::uint32_t number) {
		if (number >= m_sets.size()) { throw Error{fmt::format("OOB set number: [{}]", number)}; }
		return m_sets[number].acquire();
	}

	void release_all() {
		for (auto& set : m_sets) { set.release_all(); }
	}

	vk::Device device{};

  private:
	std::vector<SetAllocator> m_sets{};
};

template <std::size_t Buffering = buffering_v>
class HostBuffer {
  public:
	HostBuffer() = default;
	HostBuffer(vk::BufferUsageFlags usage) : m_usage(usage) {}

	void update(void const* data, std::size_t size, std::size_t count) {
		m_data.resize(size);
		std::memcpy(m_data.data(), data, size);
		m_count = static_cast<std::uint32_t>(count);
	}

	void flush(Vma const& vma, Index<Buffering> i) {
		if (m_buffer.get(i).get().size < m_data.size()) { m_buffer.get(i) = vma.make_buffer(m_usage, m_data.size(), true); }
		auto& buffer = m_buffer.get(i).get();
		std::memcpy(buffer.ptr, m_data.data(), m_data.size());
	}

	BufferView view(Index<Buffering> i) const {
		auto const& buffer = m_buffer.get(i).get();
		return BufferView{.buffer = buffer.buffer, .size = buffer.size, .count = m_count};
	}

  private:
	vk::BufferUsageFlags m_usage;
	std::vector<std::byte> m_data{};
	std::uint32_t m_count{};
	Buffered<UniqueBuffer, Buffering> m_buffer{};
};

template <typename Type, std::size_t Buffering = buffering_v>
class BufferVec {
  public:
	BufferVec() = default;
	BufferVec(vk::BufferUsageFlags usage) : m_usage(usage) {}

	template <typename F>
	BufferView write(Vma const& vma, std::size_t size, Index<Buffering> i, F for_each) {
		auto& buffer = [&]() -> HostBuffer<Buffering>& {
			if (m_index < m_buffers.size()) { return m_buffers[m_index]; }
			m_buffers.emplace_back(m_usage);
			return m_buffers.back();
		}();
		m_vec.clear();
		m_vec.resize(size);
		for (auto [t, index] : enumerate(m_vec)) { for_each(t, index); }
		auto const span = std::span<Type const>{m_vec};
		buffer.update(span.data(), span.size_bytes(), span.size());
		buffer.flush(vma, i);
		++m_index;
		return buffer.view(i);
	}

	void release() { m_index = {}; }

  private:
	using Vec = std::vector<Type>;
	std::vector<HostBuffer<Buffering>> m_buffers{};
	Vec m_vec{};
	vk::BufferUsageFlags m_usage;
	std::size_t m_index{};
};

struct PipelineState {
	vk::PolygonMode mode{vk::PolygonMode::eFill};
	vk::PrimitiveTopology topology{vk::PrimitiveTopology::eTriangleList};
	bool depth_test{true};

	bool operator==(PipelineState const&) const = default;
};

template <std::size_t Buffering = buffering_v>
struct PipelineStorage {
	struct Program {
		SpirV vert{};
		SpirV frag{};
	};

	struct Key {
		PipelineState state{};
		std::size_t spirv_hash{};
		std::size_t vertex_input_hash{};
		bool operator==(Key const&) const = default;

		static Key make(PipelineState state, VertexInput::View vinput, SpirV vert, SpirV frag) {
			return {state, make_combined_hash(vert.hash, frag.hash), vinput.hash};
		}

		struct Hasher {
			std::size_t operator()(Key const& key) const {
				return make_combined_hash(key.state.depth_test, key.state.mode, key.state.topology, key.spirv_hash, key.vertex_input_hash);
			}
		};
	};

	struct Value {
		std::unordered_map<vk::RenderPass, vk::UniquePipeline> pipelines{};
		vk::UniquePipelineLayout pipeline_layout{};
		std::vector<SetLayout> set_layouts{};
		std::vector<vk::UniqueDescriptorSetLayout> descriptor_set_layouts{};
		Buffered<SetAllocator::Pool, Buffering> set_pools{};
		vk::UniqueShaderModule vert{};
		vk::UniqueShaderModule frag{};
	};

	struct Pipe {
		vk::Pipeline pipeline{};
		vk::PipelineLayout layout{};
		Ptr<SetAllocator::Pool> set_pool{};
		std::span<SetLayout const> set_layouts{};
		explicit operator bool() const { return pipeline && layout && set_pool; }

		void bind(vk::CommandBuffer cb, vk::Extent2D extent, float line_width = 1.0f) {
			cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
			glm::vec2 const fextent = glm::uvec2{extent.width, extent.height};
			auto viewport = vk::Viewport{0.0f, fextent.y, fextent.x, -fextent.y, 0.0f, 1.0f};
			auto scissor = vk::Rect2D{{}, extent};
			cb.setViewport(0u, viewport);
			cb.setScissor(0u, scissor);
			cb.setLineWidth(line_width);
		}
	};

	std::unordered_map<Key, Value, typename Key::Hasher> map{};
	bool sample_rate_shading{};

	void populate(Value& out, Vma const& vma, SpirV vert, SpirV frag);
	vk::Pipeline get(Value& out, Vma const& vma, PipelineState st, VertexInput::View vi, RenderPassView rp, Program program);

	Pipe get(Vma const& vma, PipelineState st, VertexInput::View vi, RenderPassView rp, Program program, Index<Buffering> i) {
		if (!rp.render_pass || program.vert.hash == 0 || program.frag.hash == 0) { throw Error{"TODO: error"}; }
		auto const key = Key::make(st, vi, program.vert, program.frag);
		auto& value = map[key];
		auto ret = get(value, vma, st, vi, rp, program);
		return {ret, *value.pipeline_layout, &value.set_pools.get(i), value.set_layouts};
	}

	void release_all(Index<Buffering> i) {
		for (auto& [_, m] : map) { m.set_pools.get(i).release_all(); }
	}
};

struct SamplerStorage {
	struct Hasher {
		std::size_t operator()(TextureSampler const& sampler) const { return make_combined_hash(sampler.min, sampler.mag, sampler.wrap_s, sampler.wrap_t); }
	};

	std::unordered_map<TextureSampler, vk::UniqueSampler, Hasher> map{};
	float anisotropy{};

	vk::Sampler get(vk::Device device, TextureSampler const& sampler) {
		if (auto it = map.find(sampler); it != map.end()) { return *it->second; }
		auto sci = vk::SamplerCreateInfo{};
		sci.minFilter = from(sampler.min);
		sci.magFilter = from(sampler.mag);
		sci.anisotropyEnable = anisotropy > 0.0f;
		sci.maxAnisotropy = anisotropy;
		sci.borderColor = vk::BorderColor::eIntOpaqueBlack;
		sci.mipmapMode = vk::SamplerMipmapMode::eNearest;
		sci.addressModeU = from(sampler.wrap_s);
		sci.addressModeV = from(sampler.wrap_t);
		sci.addressModeW = from(sampler.wrap_s);
		sci.maxLod = VK_LOD_CLAMP_NONE;
		auto [it, _] = map.insert_or_assign(sampler, device.createSamplerUnique(sci));
		return *it->second;
	}
};

struct ShaderSet {
	Ptr<DescriptorSet> descriptor_set{};
	bool updated{};
};

template <std::size_t Buffering = buffering_v>
struct VulkanShader : Shader {
	vk::Device device{};
	std::span<SetLayout const> set_layouts{};
	Ptr<SamplerStorage> samplers{};
	std::array<ShaderSet, max_sets_v> sets{};

	VulkanShader(typename PipelineStorage<Buffering>::Pipe& pipe, SamplerStorage& samplers) : device(pipe.set_pool->device), samplers(&samplers) {
		for (auto const& layout : pipe.set_layouts) {
			if (layout.bindings.empty()) { continue; }
			sets[layout.set].descriptor_set = &pipe.set_pool->next_set(layout.set);
		}
	}

	void update(std::uint32_t set, std::uint32_t binding, Texture const& texture) final;
	void write(std::uint32_t set, std::uint32_t binding, void const* data, std::size_t size) final;
	void update(std::uint32_t set, std::uint32_t binding, BufferView const& buffer);
	void bind(vk::PipelineLayout layout, vk::CommandBuffer cb) const;
	Ptr<DescriptorSet> get_set(std::uint32_t set);
};

struct VertexLayout {
	VertexInput input{};
	std::string shader_uri{"shaders/default.vert"};
};

struct StaticDrawInfo {
	AssetProviders const& providers;
	MeshGeometry const& geometry;
	Material const& material;
	glm::mat4 const& transform;
	Topology topology{};
};

struct StaticMeshRenderInfo {
	AssetProviders const& providers;
	StaticMesh const& mesh;
	glm::mat4 const& parent;
	std::span<Transform const> instances;
};

struct SkinnedMeshRenderInfo {
	AssetProviders const& providers;
	SkinnedMesh const& mesh;
	std::span<glm::mat4 const> joints;
};
} // namespace

class VulkanMeshGeometry::Impl {
  public:
	struct Info {
		std::uint32_t vertices{};
		std::uint32_t indices{};
	};

	using Joints = MeshJoints;

	Impl(VulkanDevice const& device, Geometry::Packed const& geometry, Joints joints = {});

	Info info() const { return {m_vertices, m_indices}; }
	VertexLayout const& vertex_layout() const { return m_vlayout; }
	bool has_joints() const { return m_jwbo.get().get().size > 0; }
	std::uint32_t instance_binding() const { return m_instance_binding; }
	std::optional<std::uint32_t> joints_set() const { return m_jwbo.get().get().size > 0 ? std::optional<std::uint32_t>{3} : std::nullopt; }

	void draw(vk::CommandBuffer cb, std::uint32_t instances = 1u) const {
		auto const& v = m_vibo.get().get();
		vk::Buffer const buffers[] = {v.buffer, v.buffer, v.buffer, v.buffer};
		vk::DeviceSize const offsets[] = {m_offsets.positions, m_offsets.rgbs, m_offsets.normals, m_offsets.uvs};
		cb.bindVertexBuffers(0u, buffers, offsets);
		if (m_jwbo.get().get().size > 0) {
			vk::Buffer const buffers[] = {m_jwbo.get().get().buffer, m_jwbo.get().get().buffer};
			vk::DeviceSize const offsets[] = {m_offsets.joints, m_offsets.weights};
			cb.bindVertexBuffers(4u, buffers, offsets);
		}
		if (m_indices > 0) {
			cb.bindIndexBuffer(v.buffer, m_offsets.indices, vk::IndexType::eUint32);
			cb.drawIndexed(m_indices, instances, 0u, 0u, 0u);
		} else {
			cb.draw(m_vertices, instances, 0u, 0u);
		}
	}

  private:
	struct Uploader;
	struct Offsets {
		std::size_t positions{};
		std::size_t rgbs{};
		std::size_t normals{};
		std::size_t uvs{};
		std::size_t joints{};
		std::size_t weights{};
		std::size_t indices{};
	};

	VertexLayout m_vlayout{};
	Defer<UniqueBuffer> m_vibo{};
	Defer<UniqueBuffer> m_jwbo{};
	Offsets m_offsets{};
	std::uint32_t m_vertices{};
	std::uint32_t m_indices{};
	std::uint32_t m_instance_binding{};
};

VulkanMeshGeometry::VulkanMeshGeometry() noexcept = default;
VulkanMeshGeometry::VulkanMeshGeometry(VulkanMeshGeometry&&) noexcept = default;
VulkanMeshGeometry& VulkanMeshGeometry::operator=(VulkanMeshGeometry&&) noexcept = default;
VulkanMeshGeometry::~VulkanMeshGeometry() noexcept = default;

struct VulkanTexture::Impl {
	using CreateInfo = TextureCreateInfo;

	Impl(VulkanDevice const& device, Image::View image, CreateInfo const& info = {});

	ImageView view() const { return m_image.get().get().image_view(); }
	std::uint32_t mip_levels() const { return m_info.mip_levels; }
	ColourSpace colour_space() const { return is_linear(m_info.format) ? ColourSpace::eLinear : ColourSpace::eSrgb; }

	TextureSampler sampler{};

	Impl(VulkanDevice const& device, TextureSampler sampler);

	Ptr<VulkanDevice const> m_device{};
	Defer<UniqueImage> m_image{};
	ImageCreateInfo m_info{};
	vk::ImageLayout m_layout{vk::ImageLayout::eUndefined};
};

VulkanTexture::VulkanTexture() noexcept = default;
VulkanTexture::VulkanTexture(VulkanTexture&&) noexcept = default;
VulkanTexture& VulkanTexture::operator=(VulkanTexture&&) noexcept = default;
VulkanTexture::~VulkanTexture() noexcept = default;

struct VulkanDevice::Impl {
	vk::UniqueInstance instance{};
	vk::UniqueDebugUtilsMessengerEXT debug{};
	Gpu gpu{};
	vk::UniqueDevice device{};
	vk::UniqueSurfaceKHR surface{};
	vk::Queue queue{};
	std::uint32_t queue_family{};

	UniqueVma vma{};
	DeferQueue defer{};
	Swapchain swapchain{};
	OffScreen<> off_screen{};
	FsQuad<> fs_quad{};
	DearImGui dear_imgui{};
	Index<> buffered_index{};

	std::mutex mutex{};
	PipelineStorage<> pipelines{};
	SamplerStorage samplers{};

	BufferVec<glm::mat4> instance_vec{};
	BufferVec<glm::mat4> joints_vec{};
	UniqueBuffer blank_buffer{};
	HostBuffer<> view_ubo{};
	HostBuffer<> dir_lights_ssbo{};

	GraphicsDeviceInfo info{};
	RenderMode default_render_mode{};

	struct {
		struct {
			std::uint64_t draw_calls{};
		} per_frame{};
	} stats{};
};

namespace {
using namespace std::string_view_literals;
namespace logger = logger;

constexpr auto validation_layer_v = "VK_LAYER_KHRONOS_validation"sv;

class Cmd {
  public:
	Cmd(VulkanDevice const& device, vk::PipelineStageFlags wait = vk::PipelineStageFlagBits::eAllCommands)
		: m_device(device), m_allocator(CmdAllocator::make(*device.impl->device, device.impl->queue_family)), m_wait(wait) {
		cb = m_allocator.allocate(false);
		cb.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
	}

	~Cmd() {
		cb.end();
		m_fence = m_device.impl->device->createFenceUnique({});
		auto si = vk::SubmitInfo{};
		si.pWaitDstStageMask = &m_wait;
		si.commandBufferCount = 1u;
		si.pCommandBuffers = &cb;
		{
			auto lock = std::scoped_lock{m_device.impl->mutex};
			if (m_device.impl->queue.submit(1u, &si, *m_fence) != vk::Result::eSuccess) { return; }
		}
		wait(*m_device.impl->device, *m_fence);
	}

	Cmd& operator=(Cmd&&) = delete;

	vk::CommandBuffer cb{};

  private:
	VulkanDevice const& m_device;
	CmdAllocator m_allocator{};
	vk::PipelineStageFlags m_wait{};
	vk::UniqueFence m_fence{};
};

vk::UniqueInstance make_instance(std::vector<char const*> extensions, GraphicsDeviceCreateInfo const& gdci, GraphicsDeviceInfo& out) {
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
	for (auto const* ext : required_extensions_v) { extensions.insert(ext); }
	auto const available_extensions = gpu.device.enumerateDeviceExtensionProperties();
	for (auto const* ext : desired_extensions_v) {
		auto const found = [ext](vk::ExtensionProperties const& e) { return std::string_view{e.extensionName} == ext; };
		if (std::find_if(available_extensions.begin(), available_extensions.end(), found) != available_extensions.end()) { extensions.insert(ext); }
	}

	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	dci.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
	dci.ppEnabledLayerNames = layers.data();
	dci.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
	dci.ppEnabledExtensionNames = extensions.span().data();
	dci.pEnabledFeatures = &enabled;
	auto ret = gpu.device.createDeviceUnique(dci);
	if (ret) { VULKAN_HPP_DEFAULT_DISPATCHER.init(ret.get()); }
	return ret;
}

UniqueVma make_vma(vk::Instance instance, vk::PhysicalDevice gpu, vk::Device device) {
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
	auto ret = UniqueVma{};
	if (vmaCreateAllocator(&vaci, &ret.get().allocator) != VK_SUCCESS) { throw Error{"Failed to create Vulkan Memory Allocator"}; }
	ret.get().device = device;
	return ret;
}

Swapchain::Formats make_swapchain_formats(std::span<vk::SurfaceFormatKHR const> available) {
	auto ret = Swapchain::Formats{};
	for (auto const format : available) {
		if (format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			if (std::find(std::begin(srgb_formats_v), std::end(srgb_formats_v), format.format) != std::end(srgb_formats_v)) {
				ret.srgb.push_back(format.format);
			} else {
				ret.linear.push_back(format.format);
			}
		}
	}
	return ret;
}

vk::PresentModeKHR ideal_present_mode(std::unordered_set<vk::PresentModeKHR> const& available, vk::PresentModeKHR desired) {
	if (available.contains(desired)) { return desired; }
	if (available.contains(vk::PresentModeKHR::eFifoRelaxed)) { return vk::PresentModeKHR::eFifoRelaxed; }
	assert(available.contains(vk::PresentModeKHR::eFifo));
	return vk::PresentModeKHR::eFifo;
}

vk::SurfaceFormatKHR surface_format(Swapchain::Formats const& formats, ColourSpace colour_space) {
	if (colour_space == ColourSpace::eSrgb && !formats.srgb.empty()) { return formats.srgb.front(); }
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

constexpr Vsync::Type from(vk::PresentModeKHR const mode) {
	switch (mode) {
	case vk::PresentModeKHR::eMailbox: return Vsync::eMailbox;
	case vk::PresentModeKHR::eImmediate: return Vsync::eOff;
	case vk::PresentModeKHR::eFifoRelaxed: return Vsync::eAdaptive;
	default: return Vsync::eOn;
	}
}

[[maybe_unused]] constexpr AntiAliasing::Type from(vk::SampleCountFlagBits const samples) {
	switch (samples) {
	case vk::SampleCountFlagBits::e32: return AntiAliasing::e32x;
	case vk::SampleCountFlagBits::e16: return AntiAliasing::e16x;
	case vk::SampleCountFlagBits::e8: return AntiAliasing::e8x;
	case vk::SampleCountFlagBits::e4: return AntiAliasing::e4x;
	case vk::SampleCountFlagBits::e2: return AntiAliasing::e2x;
	default: return AntiAliasing::Type::e1x;
	}
}

constexpr vk::SampleCountFlagBits from(AntiAliasing::Type const aa) {
	switch (aa) {
	case AntiAliasing::e32x: return vk::SampleCountFlagBits::e32;
	case AntiAliasing::e16x: return vk::SampleCountFlagBits::e16;
	case AntiAliasing::e8x: return vk::SampleCountFlagBits::e8;
	case AntiAliasing::e4x: return vk::SampleCountFlagBits::e4;
	case AntiAliasing::e2x: return vk::SampleCountFlagBits::e2;
	default: return vk::SampleCountFlagBits::e1;
	}
}

constexpr vk::PresentModeKHR from(Vsync::Type const type) {
	switch (type) {
	case Vsync::eMailbox: return vk::PresentModeKHR::eMailbox;
	case Vsync::eOff: return vk::PresentModeKHR::eImmediate;
	case Vsync::eAdaptive: return vk::PresentModeKHR::eFifoRelaxed;
	default: return vk::PresentModeKHR::eFifo;
	}
}

constexpr vk::PolygonMode from(RenderMode::Type const mode) {
	switch (mode) {
	case RenderMode::Type::ePoint: return vk::PolygonMode::ePoint;
	case RenderMode::Type::eLine: return vk::PolygonMode::eLine;
	default: return vk::PolygonMode::eFill;
	}
}

constexpr vk::PrimitiveTopology from(Topology const topo) {
	switch (topo) {
	case Topology::ePointList: return vk::PrimitiveTopology::ePointList;
	case Topology::eLineList: return vk::PrimitiveTopology::eLineList;
	case Topology::eLineStrip: return vk::PrimitiveTopology::eLineStrip;
	case Topology::eTriangleStrip: return vk::PrimitiveTopology::eTriangleStrip;
	default: return vk::PrimitiveTopology::eTriangleList;
	}
}

Vsync make_vsync(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
	auto const supported_modes = device.getSurfacePresentModesKHR(surface);
	auto ret = Vsync{};
	for (auto const mode : supported_modes) { ret.flags |= from(mode); }
	return ret;
}

AntiAliasing make_aa(vk::PhysicalDeviceProperties const& props) {
	auto ret = AntiAliasing{};
	auto const supported = props.limits.framebufferColorSampleCounts;
	if (supported & vk::SampleCountFlagBits::e32) { ret.flags |= AntiAliasing::e32x; }
	if (supported & vk::SampleCountFlagBits::e16) { ret.flags |= AntiAliasing::e16x; }
	if (supported & vk::SampleCountFlagBits::e8) { ret.flags |= AntiAliasing::e8x; }
	if (supported & vk::SampleCountFlagBits::e4) { ret.flags |= AntiAliasing::e4x; }
	if (supported & vk::SampleCountFlagBits::e2) { ret.flags |= AntiAliasing::e2x; }
	if (supported & vk::SampleCountFlagBits::e1) { ret.flags |= AntiAliasing::e1x; }
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

vk::SwapchainCreateInfoKHR make_swci(std::uint32_t queue_family, vk::SurfaceKHR surface, Swapchain::Formats const& formats, ColourSpace colour_space,
									 vk::PresentModeKHR mode) {
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

vk::Result make_swapchain(VulkanDevice const& device, std::optional<glm::uvec2> extent, std::optional<vk::PresentModeKHR> mode) {
	auto create_info = device.impl->swapchain.info;
	if (mode) {
		assert(device.impl->swapchain.modes.contains(*mode));
		create_info.presentMode = *mode;
	}
	auto const caps = device.impl->gpu.device.getSurfaceCapabilitiesKHR(*device.impl->surface);
	if (extent) { create_info.imageExtent = image_extent(caps, vk::Extent2D{extent->x, extent->y}); }

	create_info.minImageCount = image_count(caps);
	create_info.oldSwapchain = device.impl->swapchain.storage.swapchain.get();
	auto vk_swapchain = vk::SwapchainKHR{};
	auto const ret = device.impl->device->createSwapchainKHR(&create_info, nullptr, &vk_swapchain);
	if (ret != vk::Result::eSuccess) { return ret; }
	device.impl->swapchain.info = std::move(create_info);
	if (device.impl->swapchain.storage.swapchain) { device.impl->defer.push(std::move(device.impl->swapchain.storage)); }
	device.impl->swapchain.storage.swapchain = vk::UniqueSwapchainKHR{vk_swapchain, *device.impl->device};
	auto const images = device.impl->device->getSwapchainImagesKHR(*device.impl->swapchain.storage.swapchain);
	device.impl->swapchain.storage.images.clear();
	device.impl->swapchain.storage.views.clear();
	for (auto const image : images) {
		device.impl->swapchain.storage.views.insert(device.impl->vma.get().make_image_view(image, device.impl->swapchain.info.imageFormat));
		device.impl->swapchain.storage.images.insert({image, *device.impl->swapchain.storage.views.span().back(), device.impl->swapchain.info.imageExtent});
	}

	logger::info("[Swapchain] extent: [{}x{}] | images: [{}] | colour space: [{}] | vsync: [{}]", create_info.imageExtent.width, create_info.imageExtent.height,
				 device.impl->swapchain.storage.images.size(), is_srgb(device.impl->swapchain.info.imageFormat) ? "sRGB" : "linear",
				 vsync_status(device.impl->swapchain.info.presentMode));
	return ret;
}

template <std::size_t Buffering>
OffScreen<Buffering> OffScreen<Buffering>::make(VulkanDevice const& device, vk::SampleCountFlagBits samples, vk::Format depth, vk::Format colour) {
	auto attachments = FlexArray<vk::AttachmentDescription, 4>{};
	auto colour_ref = vk::AttachmentReference{};
	auto depth_ref = vk::AttachmentReference{};
	auto resolve_ref = vk::AttachmentReference{};

	auto subpass = vk::SubpassDescription{};
	subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colour_ref;

	static constexpr auto final_layout = vk::ImageLayout::eShaderReadOnlyOptimal;

	colour_ref.attachment = static_cast<std::uint32_t>(attachments.size());
	colour_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;
	auto attachment = vk::AttachmentDescription{};
	attachment.format = colour;
	attachment.samples = samples;
	attachment.loadOp = vk::AttachmentLoadOp::eClear;
	attachment.storeOp = samples > vk::SampleCountFlagBits::e1 ? vk::AttachmentStoreOp::eDontCare : vk::AttachmentStoreOp::eStore;
	attachment.initialLayout = vk::ImageLayout::eUndefined;
	attachment.finalLayout = samples > vk::SampleCountFlagBits::e1 ? colour_ref.layout : final_layout;
	attachments.insert(attachment);

	if (depth != vk::Format::eUndefined) {
		subpass.pDepthStencilAttachment = &depth_ref;
		depth_ref.attachment = static_cast<std::uint32_t>(attachments.size());
		depth_ref.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
		attachment = vk::AttachmentDescription{};
		attachment.format = depth;
		attachment.samples = samples;
		attachment.loadOp = vk::AttachmentLoadOp::eClear;
		attachment.storeOp = vk::AttachmentStoreOp::eDontCare;
		attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachment.initialLayout = vk::ImageLayout::eUndefined;
		attachment.finalLayout = depth_ref.layout;
		attachments.insert(attachment);
	}

	if (samples > vk::SampleCountFlagBits::e1) {
		subpass.pResolveAttachments = &resolve_ref;
		resolve_ref.attachment = static_cast<std::uint32_t>(attachments.size());
		resolve_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

		attachment = attachments.span().front();
		attachment.samples = vk::SampleCountFlagBits::e1;
		attachment.loadOp = vk::AttachmentLoadOp::eDontCare;
		attachment.storeOp = vk::AttachmentStoreOp::eStore;
		attachment.initialLayout = vk::ImageLayout::eUndefined;
		attachment.finalLayout = final_layout;
		attachments.insert(attachment);
	}

	auto rpci = vk::RenderPassCreateInfo{};
	rpci.attachmentCount = static_cast<std::uint32_t>(attachments.size());
	rpci.pAttachments = attachments.data();
	rpci.subpassCount = 1u;
	rpci.pSubpasses = &subpass;
	auto sds = FlexArray<vk::SubpassDependency, 2>{};
	auto sd = vk::SubpassDependency{};
	sd.srcSubpass = VK_SUBPASS_EXTERNAL;
	sd.srcStageMask = vk::PipelineStageFlagBits::eFragmentShader;
	sd.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	if (depth != vk::Format{}) { sd.dstStageMask |= vk::PipelineStageFlagBits::eEarlyFragmentTests; }
	sd.srcAccessMask = vk::AccessFlagBits::eShaderRead;
	sd.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
	if (depth != vk::Format{}) { sd.dstAccessMask |= vk::AccessFlagBits::eDepthStencilAttachmentWrite; }
	sds.insert(sd);

	std::swap(sd.srcSubpass, sd.dstSubpass);
	std::swap(sd.srcAccessMask, sd.dstAccessMask);
	std::swap(sd.srcStageMask, sd.dstStageMask);
	sds.insert(sd);

	rpci.dependencyCount = static_cast<std::uint32_t>(sds.size());
	rpci.pDependencies = sds.data();

	auto ret = OffScreen<Buffering>{};
	ret.rp.render_pass = device.impl->device->createRenderPassUnique(rpci);
	ret.rp.samples = samples;
	ret.rp.colour = colour;
	ret.rp.depth = depth;

	for (auto& frame : ret.rp.frames.t) { frame = RenderFrame::make(device); }
	return ret;
}

vk::UniqueRenderPass make_fs_quad_pass(VulkanDevice const& device, vk::Format colour) {
	auto colour_ref = vk::AttachmentReference{};
	auto subpass = vk::SubpassDescription{};
	subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colour_ref;
	colour_ref.attachment = 0u;
	colour_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;
	auto attachment = vk::AttachmentDescription{};
	attachment.format = colour;
	attachment.samples = vk::SampleCountFlagBits::e1;
	attachment.loadOp = vk::AttachmentLoadOp::eDontCare;
	attachment.storeOp = vk::AttachmentStoreOp::eStore;
	attachment.initialLayout = vk::ImageLayout::eUndefined;
	attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

	auto rpci = vk::RenderPassCreateInfo{};
	rpci.attachmentCount = 1u;
	rpci.pAttachments = &attachment;
	rpci.subpassCount = 1u;
	rpci.pSubpasses = &subpass;
	auto sds = FlexArray<vk::SubpassDependency, 2>{};
	auto sd = vk::SubpassDependency{};
	sd.srcSubpass = VK_SUBPASS_EXTERNAL;
	sd.srcStageMask = vk::PipelineStageFlagBits::eFragmentShader;
	sd.dstStageMask =
		vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests;
	sd.srcAccessMask = vk::AccessFlagBits::eShaderRead;
	sd.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
	sds.insert(sd);

	std::swap(sd.srcSubpass, sd.dstSubpass);
	std::swap(sd.srcAccessMask, sd.dstAccessMask);
	std::swap(sd.srcStageMask, sd.dstStageMask);
	sds.insert(sd);

	rpci.dependencyCount = static_cast<std::uint32_t>(sds.size());
	rpci.pDependencies = sds.data();

	return device.impl->device->createRenderPassUnique(rpci);
}

template <std::size_t Buffering>
Framebuffer OffScreen<Buffering>::make_framebuffer(VulkanDevice const& device, vk::Extent2D extent, float scale) {
	extent = scale_extent(extent, scale);
	if (output_image.get().get().extent != extent) {
		auto const ici = ImageCreateInfo{
			.format = rp.colour,
			.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
			.aspect = vk::ImageAspectFlagBits::eColor,
			.samples = vk::SampleCountFlagBits::e1,
		};
		output_image = {device.impl->defer, device.impl->vma.get().make_image(ici, extent)};
	}
	auto const& colour_output = output_image.get().get().image_view();
	auto ret = Framebuffer{};
	ret.render_target.colour = colour_output;
	if (rp.samples > vk::SampleCountFlagBits::e1) {
		if (msaa_image.get().get().extent != colour_output.extent) {
			auto const ici = ImageCreateInfo{
				.format = rp.colour,
				.usage = vk::ImageUsageFlagBits::eColorAttachment,
				.aspect = vk::ImageAspectFlagBits::eColor,
				.samples = rp.samples,
			};
			msaa_image = {device.impl->defer, device.impl->vma.get().make_image(ici, colour_output.extent)};
		}
		ret.render_target.colour = msaa_image.get().get().image_view();
		ret.render_target.resolve = colour_output;
	}
	if (rp.depth != vk::Format::eUndefined) {
		if (depth_buffer.get().get().extent != colour_output.extent) {
			auto const ici = ImageCreateInfo{
				.format = rp.depth,
				.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
				.aspect = vk::ImageAspectFlagBits::eDepth,
				.samples = rp.samples,
			};
			depth_buffer = {device.impl->defer, device.impl->vma.get().make_image(ici, colour_output.extent)};
		}
		ret.render_target.depth = depth_buffer.get().get().image_view();
	}
	ret.render_target.extent = colour_output.extent;

	auto fbci = vk::FramebufferCreateInfo{};
	fbci.renderPass = *rp.render_pass;
	auto const attachments = ret.render_target.attachments();
	fbci.attachmentCount = static_cast<std::uint32_t>(attachments.size());
	fbci.pAttachments = attachments.span().data();
	fbci.width = ret.render_target.colour.extent.width;
	fbci.height = ret.render_target.colour.extent.height;
	fbci.layers = 1;
	ret.framebuffer = device.impl->device->createFramebufferUnique(fbci);
	return ret;
}

template <std::size_t Buffering>
Framebuffer FsQuad<Buffering>::make_framebuffer(VulkanDevice const& device, ImageView const& output_image) {
	auto ret = Framebuffer{};
	ret.render_target.colour = output_image;
	ret.render_target.extent = output_image.extent;

	auto fbci = vk::FramebufferCreateInfo{};
	fbci.renderPass = *rp;
	auto const attachments = ret.render_target.attachments();
	fbci.attachmentCount = static_cast<std::uint32_t>(attachments.size());
	fbci.pAttachments = attachments.span().data();
	fbci.width = ret.render_target.colour.extent.width;
	fbci.height = ret.render_target.colour.extent.height;
	fbci.layers = 1;
	ret.framebuffer = device.impl->device->createFramebufferUnique(fbci);
	return ret;
}

constexpr auto get_samples(AntiAliasing supported, AntiAliasing::Type const desired) {
	if (desired >= AntiAliasing::e32x && (supported.flags & AntiAliasing::e32x)) { return AntiAliasing::e32x; }
	if (desired >= AntiAliasing::e16x && (supported.flags & AntiAliasing::e16x)) { return AntiAliasing::e16x; }
	if (desired >= AntiAliasing::e8x && (supported.flags & AntiAliasing::e8x)) { return AntiAliasing::e8x; }
	if (desired >= AntiAliasing::e4x && (supported.flags & AntiAliasing::e4x)) { return AntiAliasing::e4x; }
	if (desired >= AntiAliasing::e2x && (supported.flags & AntiAliasing::e2x)) { return AntiAliasing::e2x; }
	return AntiAliasing::e1x;
}

vk::Format depth_format(vk::PhysicalDevice const gpu) {
	static constexpr auto target{vk::Format::eD32Sfloat};
	auto const props = gpu.getFormatProperties(target);
	if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) { return target; }
	return vk::Format::eD16Unorm;
}

DearImGui make_imgui(Window const& window, VulkanDevice const& device) {
	auto ret = DearImGui{};
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
	ret.pool = device.impl->device->createDescriptorPoolUnique(dpci);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

	ImGui::StyleColorsDark();
	if (device.impl->info.swapchain == ColourSpace::eSrgb) {
		auto* colours = ImGui::GetStyle().Colors;
		for (int i = 0; i < ImGuiCol_COUNT; ++i) {
			auto& colour = colours[i];
			auto const corrected = glm::convertSRGBToLinear(glm::vec4{colour.x, colour.y, colour.z, colour.w});
			colour = {corrected.x, corrected.y, corrected.z, corrected.w};
		}
	}

	auto const* desktop_window = window.as<DesktopWindow>();
	if (!desktop_window) { throw Error{"TODO: error"}; }
	auto loader = vk::DynamicLoader{};
	auto get_fn = [&loader](char const* name) { return loader.getProcAddress<PFN_vkVoidFunction>(name); };
	auto lambda = +[](char const* name, void* ud) {
		auto const* gf = reinterpret_cast<decltype(get_fn)*>(ud);
		return (*gf)(name);
	};
	ImGui_ImplVulkan_LoadFunctions(lambda, &get_fn);
	ImGui_ImplGlfw_InitForVulkan(desktop_window->window, true);
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = *device.impl->instance;
	init_info.PhysicalDevice = device.impl->gpu.device;
	init_info.Device = *device.impl->device;
	init_info.QueueFamily = device.impl->queue_family;
	init_info.Queue = device.impl->queue;
	init_info.DescriptorPool = *ret.pool;
	init_info.Subpass = 0;
	init_info.MinImageCount = 2;
	init_info.ImageCount = 2;
	init_info.MSAASamples = static_cast<VkSampleCountFlagBits>(vk::SampleCountFlagBits::e1);

	ImGui_ImplVulkan_Init(&init_info, *device.impl->fs_quad.rp);

	{
		auto cmd = Cmd{device};
		ImGui_ImplVulkan_CreateFontsTexture(cmd.cb);
	}
	ImGui_ImplVulkan_DestroyFontUploadObjects();

	return ret;
}

[[maybe_unused]] constexpr bool has_all_sets(std::span<SetLayout const> layouts) {
	std::uint32_t set{};
	for (auto const& layout : layouts) {
		if (layout.set != set) { return false; }
		++set;
	}
	return true;
}

std::vector<SetLayout> make_set_layouts(std::span<std::uint32_t const> vert, std::span<std::uint32_t const> frag) {
	std::map<std::uint32_t, std::map<std::uint32_t, vk::DescriptorSetLayoutBinding>> set_layout_bindings{};
	auto populate = [&set_layout_bindings](std::span<std::uint32_t const> code, vk::ShaderStageFlagBits stage) {
		auto compiler = spirv_cross::CompilerGLSL{code.data(), code.size()};
		auto resources = compiler.get_shader_resources();
		auto set_resources = [&compiler, &set_layout_bindings, stage](std::span<spirv_cross::Resource const> resources, vk::DescriptorType const type) {
			for (auto const& resource : resources) {
				auto const set_number = compiler.get_decoration(resource.id, spv::Decoration::DecorationDescriptorSet);
				auto& layout = set_layout_bindings[set_number];
				auto const binding_number = compiler.get_decoration(resource.id, spv::Decoration::DecorationBinding);
				auto& dslb = layout[binding_number];
				dslb.binding = binding_number;
				dslb.descriptorType = type;
				dslb.stageFlags |= stage;
				auto const& type = compiler.get_type(resource.type_id);
				if (type.array.size() == 0) {
					dslb.descriptorCount = std::max(dslb.descriptorCount, 1u);
				} else {
					dslb.descriptorCount = type.array[0];
				}
			}
		};
		set_resources(resources.uniform_buffers, vk::DescriptorType::eUniformBuffer);
		set_resources(resources.storage_buffers, vk::DescriptorType::eStorageBuffer);
		set_resources(resources.sampled_images, vk::DescriptorType::eCombinedImageSampler);
	};
	populate(vert, vk::ShaderStageFlagBits::eVertex);
	populate(frag, vk::ShaderStageFlagBits::eFragment);

	auto ret = std::vector<SetLayout>{};
	auto next_set = std::uint32_t{};
	for (auto& [set, layouts] : set_layout_bindings) {
		while (next_set < set) {
			ret.push_back({});
			++next_set;
		}
		next_set = set + 1;
		auto layout = SetLayout{.set = set};
		for (auto const& [binding, dslb] : layouts) { layout.bindings.insert(dslb); }
		ret.push_back(std::move(layout));
	}
	assert(std::is_sorted(ret.begin(), ret.end(), [](auto const& a, auto const& b) { return a.set < b.set; }));
	assert(has_all_sets(ret));
	return ret;
}

struct PipeInfo {
	VertexInput::View vinput;
	vk::ShaderModule vert;
	vk::ShaderModule frag;
	vk::PipelineLayout layout;

	RenderPassView render_pass;
	PipelineState state{};
	bool sample_shading{true};
};

vk::UniqueShaderModule make_shader_module(vk::Device device, SpirV const& spirv) {
	assert(spirv.hash > 0 && !spirv.code.empty());
	auto smci = vk::ShaderModuleCreateInfo{{}, spirv.code.size_bytes(), spirv.code.data()};
	return device.createShaderModuleUnique(smci);
}

vk::UniquePipelineLayout make_pipeline_layout(vk::Device device, std::span<vk::UniqueDescriptorSetLayout const> set_layouts) {
	auto layouts = std::vector<vk::DescriptorSetLayout>{};
	layouts.reserve(set_layouts.size());
	for (auto const& layout : set_layouts) { layouts.push_back(*layout); }
	auto plci = vk::PipelineLayoutCreateInfo{};
	plci.setLayoutCount = static_cast<std::uint32_t>(set_layouts.size());
	plci.pSetLayouts = layouts.data();
	return device.createPipelineLayoutUnique(plci);
}

vk::UniquePipeline make_pipeline(vk::Device device, PipeInfo const& info) {
	auto gpci = vk::GraphicsPipelineCreateInfo{};
	gpci.renderPass = info.render_pass.render_pass;
	gpci.layout = info.layout;

	auto pvisci = vk::PipelineVertexInputStateCreateInfo{};
	pvisci.pVertexBindingDescriptions = info.vinput.bindings.data();
	pvisci.vertexBindingDescriptionCount = static_cast<std::uint32_t>(info.vinput.bindings.size());
	pvisci.pVertexAttributeDescriptions = info.vinput.attributes.data();
	pvisci.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(info.vinput.attributes.size());
	gpci.pVertexInputState = &pvisci;

	auto pssciVert = vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, info.vert, "main");
	auto pssciFrag = vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, info.frag, "main");
	auto psscis = std::array<vk::PipelineShaderStageCreateInfo, 2>{{pssciVert, pssciFrag}};
	gpci.stageCount = static_cast<std::uint32_t>(psscis.size());
	gpci.pStages = psscis.data();

	auto piasci = vk::PipelineInputAssemblyStateCreateInfo({}, info.state.topology);
	gpci.pInputAssemblyState = &piasci;

	auto prsci = vk::PipelineRasterizationStateCreateInfo();
	prsci.polygonMode = info.state.mode;
	prsci.cullMode = vk::CullModeFlagBits::eNone;
	gpci.pRasterizationState = &prsci;

	auto pdssci = vk::PipelineDepthStencilStateCreateInfo{};
	pdssci.depthTestEnable = pdssci.depthWriteEnable = info.state.depth_test;
	pdssci.depthCompareOp = vk::CompareOp::eLess;
	gpci.pDepthStencilState = &pdssci;

	auto pcbas = vk::PipelineColorBlendAttachmentState{};
	using CCF = vk::ColorComponentFlagBits;
	pcbas.colorWriteMask = CCF::eR | CCF::eG | CCF::eB | CCF::eA;
	pcbas.blendEnable = true;
	pcbas.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
	pcbas.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
	pcbas.colorBlendOp = vk::BlendOp::eAdd;
	pcbas.srcAlphaBlendFactor = vk::BlendFactor::eOne;
	pcbas.dstAlphaBlendFactor = vk::BlendFactor::eZero;
	pcbas.alphaBlendOp = vk::BlendOp::eAdd;
	auto pcbsci = vk::PipelineColorBlendStateCreateInfo();
	pcbsci.attachmentCount = 1;
	pcbsci.pAttachments = &pcbas;
	gpci.pColorBlendState = &pcbsci;

	auto pdsci = vk::PipelineDynamicStateCreateInfo();
	vk::DynamicState const pdscis[] = {vk::DynamicState::eViewport, vk::DynamicState::eScissor, vk::DynamicState::eLineWidth};
	pdsci = vk::PipelineDynamicStateCreateInfo({}, static_cast<std::uint32_t>(std::size(pdscis)), pdscis);
	gpci.pDynamicState = &pdsci;

	auto pvsci = vk::PipelineViewportStateCreateInfo({}, 1, {}, 1);
	gpci.pViewportState = &pvsci;

	auto pmssci = vk::PipelineMultisampleStateCreateInfo{};
	pmssci.rasterizationSamples = info.render_pass.samples;
	pmssci.sampleShadingEnable = info.sample_shading;
	gpci.pMultisampleState = &pmssci;

	auto ret = vk::Pipeline{};
	if (device.createGraphicsPipelines({}, 1u, &gpci, {}, &ret) != vk::Result::eSuccess) { throw Error{"Failed to create graphics pipeline"}; }

	return vk::UniquePipeline{ret, device};
}

template <std::size_t Buffering>
void PipelineStorage<Buffering>::populate(Value& out, Vma const& vma, SpirV vert, SpirV frag) {
	if (out.pipeline_layout) { return; }
	out.set_layouts = make_set_layouts(vert.code, frag.code);
	for (auto const& layout : out.set_layouts) {
		auto const dslci = vk::DescriptorSetLayoutCreateInfo{{}, static_cast<std::uint32_t>(layout.bindings.span().size()), layout.bindings.span().data()};
		out.descriptor_set_layouts.push_back(vma.device.createDescriptorSetLayoutUnique(dslci));
	}
	out.pipeline_layout = make_pipeline_layout(vma.device, out.descriptor_set_layouts);
	out.vert = make_shader_module(vma.device, vert);
	out.frag = make_shader_module(vma.device, frag);
	if (!out.pipeline_layout || !out.vert || !out.frag) { throw Error{"TODO: error"}; }
	for (auto& pool : out.set_pools.t) { pool = {vma, out.set_layouts}; }
}

template <std::size_t Buffering>
vk::Pipeline PipelineStorage<Buffering>::get(Value& out, Vma const& vma, PipelineState st, VertexInput::View vi, RenderPassView rp, Program program) {
	if (auto it = out.pipelines.find(rp.render_pass); it != out.pipelines.end()) { return *it->second; }
	if (!out.pipeline_layout) { populate(out, vma, program.vert, program.frag); }
	auto const pi = PipeInfo{
		vi, *out.vert, *out.frag, *out.pipeline_layout, rp, st, sample_rate_shading,
	};
	auto ret = make_pipeline(vma.device, pi);
	if (!ret) { throw Error{"TODO: error"}; }
	auto [it, _] = out.pipelines.insert_or_assign(rp.render_pass, std::move(ret));
	return *it->second;
}

template <std::size_t Buffering>
void VulkanShader<Buffering>::update(std::uint32_t set, std::uint32_t binding, Texture const& texture) {
	auto const* vt = texture.as<VulkanTexture>();
	assert(vt);
	get_set(set)->update(binding, vt->impl->view(), samplers->get(device, vt->impl->sampler));
}

template <std::size_t Buffering>
void VulkanShader<Buffering>::update(std::uint32_t set, std::uint32_t binding, BufferView const& buffer) {
	get_set(set)->update(binding, buffer);
}
template <std::size_t Buffering>
void VulkanShader<Buffering>::write(std::uint32_t set, std::uint32_t binding, void const* data, std::size_t size) {
	get_set(set)->write(binding, data, size);
}

template <std::size_t Buffering>
Ptr<DescriptorSet> VulkanShader<Buffering>::get_set(std::uint32_t set) {
	assert(set < max_sets_v && sets[set].descriptor_set);
	auto& ret = sets[set];
	ret.updated = true;
	return ret.descriptor_set;
}

template <std::size_t Buffering>
void VulkanShader<Buffering>::bind(vk::PipelineLayout layout, vk::CommandBuffer cb) const {
	for (auto const& set : sets) {
		if (!set.descriptor_set) { continue; }
		if (!set.updated) { logger::warn("[Shader] All descriptor sets not updated for bound pipeline"); }
		cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, set.descriptor_set->number(), set.descriptor_set->set(), {});
	}
}

constexpr auto v_flags_v = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
constexpr auto vi_flags_v = v_flags_v | vk::BufferUsageFlagBits::eIndexBuffer;

struct Writer {
	UniqueBuffer& out;
	std::size_t offset{};

	template <typename T>
	std::size_t operator()(std::span<T> data) {
		auto ret = offset;
		std::memcpy(static_cast<std::byte*>(out.get().ptr) + offset, data.data(), data.size_bytes());
		offset += data.size_bytes();
		return ret;
	}
};

VertexLayout common_vertex_layout() {
	auto ret = VertexLayout{};
	// position
	ret.input.bindings.insert(vk::VertexInputBindingDescription{0, sizeof(glm::vec3)});
	ret.input.attributes.insert(vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32B32Sfloat});
	// rgb
	ret.input.bindings.insert(vk::VertexInputBindingDescription{1, sizeof(glm::vec3)});
	ret.input.attributes.insert(vk::VertexInputAttributeDescription{1, 1, vk::Format::eR32G32B32Sfloat});
	// normal
	ret.input.bindings.insert(vk::VertexInputBindingDescription{2, sizeof(glm::vec3)});
	ret.input.attributes.insert(vk::VertexInputAttributeDescription{2, 2, vk::Format::eR32G32B32Sfloat});
	// uv
	ret.input.bindings.insert(vk::VertexInputBindingDescription{3, sizeof(glm::vec2)});
	ret.input.attributes.insert(vk::VertexInputAttributeDescription{3, 3, vk::Format::eR32G32Sfloat});
	return ret;
}

VertexLayout instanced_vertex_layout() {
	auto ret = common_vertex_layout();

	// instance matrix
	ret.input.bindings.insert(vk::VertexInputBindingDescription{6, sizeof(glm::mat4), vk::VertexInputRate::eInstance});
	ret.input.attributes.insert(vk::VertexInputAttributeDescription{6, 6, vk::Format::eR32G32B32A32Sfloat, 0 * sizeof(glm::vec4)});
	ret.input.attributes.insert(vk::VertexInputAttributeDescription{7, 6, vk::Format::eR32G32B32A32Sfloat, 1 * sizeof(glm::vec4)});
	ret.input.attributes.insert(vk::VertexInputAttributeDescription{8, 6, vk::Format::eR32G32B32A32Sfloat, 2 * sizeof(glm::vec4)});
	ret.input.attributes.insert(vk::VertexInputAttributeDescription{9, 6, vk::Format::eR32G32B32A32Sfloat, 3 * sizeof(glm::vec4)});

	ret.shader_uri = "shaders/default.vert";

	return ret;
}

VertexLayout skinned_vertex_layout() {
	auto ret = common_vertex_layout();

	// joints
	ret.input.bindings.insert(vk::VertexInputBindingDescription{4, sizeof(glm::uvec4)});
	ret.input.attributes.insert(vk::VertexInputAttributeDescription{4, 4, vk::Format::eR32G32B32A32Uint});
	// weights
	ret.input.bindings.insert(vk::VertexInputBindingDescription{5, sizeof(glm::vec4)});
	ret.input.attributes.insert(vk::VertexInputAttributeDescription{5, 5, vk::Format::eR32G32B32A32Sfloat});

	ret.shader_uri = "shaders/skinned.vert";

	return ret;
}

ImageBarrier full_barrier(Vma::Image const& image, vk::ImageLayout const src, vk::ImageLayout const dst) {
	auto ret = ImageBarrier{};
	ret.barrier.srcAccessMask = ret.barrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
	ret.barrier.oldLayout = src;
	ret.barrier.newLayout = dst;
	ret.barrier.image = image.image;
	ret.barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, image.mip_levels, 0, image.array_layers};
	ret.stages = {vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands};
	return ret;
}

struct MipMapWriter {
	vk::Image image;
	vk::Extent2D extent;
	vk::CommandBuffer cb;
	std::uint32_t mip_levels;

	std::uint32_t layer_count{1};
	ImageBarrier ib{};

	void blit_mips(std::uint32_t const src_level, vk::Offset3D const src_offset, vk::Offset3D const dst_offset) const {
		auto const src_isrl = vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, src_level, 0, layer_count};
		auto const dst_isrl = vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, src_level + 1, 0, layer_count};
		auto const region = vk::ImageBlit{src_isrl, {vk::Offset3D{}, src_offset}, dst_isrl, {vk::Offset3D{}, dst_offset}};
		cb.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image, vk::ImageLayout::eTransferDstOptimal, region, vk::Filter::eLinear);
	}

	void blit_next_mip(std::uint32_t const src_level, vk::Offset3D const src_offset, vk::Offset3D const dst_offset) {
		ib.barrier.oldLayout = vk::ImageLayout::eUndefined;
		ib.barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
		ib.barrier.subresourceRange.baseMipLevel = src_level + 1;
		ib.transition(cb);

		blit_mips(src_level, src_offset, dst_offset);

		ib.barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		ib.barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
		ib.transition(cb);
	}

	void operator()() {
		ib.barrier.image = image;
		ib.barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		ib.barrier.subresourceRange.baseArrayLayer = 0;
		ib.barrier.subresourceRange.layerCount = layer_count;
		ib.barrier.subresourceRange.baseMipLevel = 0;
		ib.barrier.subresourceRange.levelCount = 1;

		ib.barrier.srcAccessMask = ib.barrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
		ib.stages = {vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer};

		ib.barrier.oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		ib.barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
		ib.barrier.subresourceRange.baseMipLevel = 0;
		ib.transition(cb);

		ib.stages = {vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer};

		auto src_extent = vk::Extent3D{extent, 1};
		for (std::uint32_t mip = 0; mip + 1 < mip_levels; ++mip) {
			vk::Extent3D dst_extent = vk::Extent3D(std::max(src_extent.width / 2, 1u), std::max(src_extent.height / 2, 1u), 1u);
			auto const src_offset = vk::Offset3D{static_cast<int>(src_extent.width), static_cast<int>(src_extent.height), 1};
			auto const dst_offset = vk::Offset3D{static_cast<int>(dst_extent.width), static_cast<int>(dst_extent.height), 1};
			blit_next_mip(mip, src_offset, dst_offset);
			src_extent = dst_extent;
		}

		ib.barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
		ib.barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		ib.barrier.subresourceRange.baseMipLevel = 0;
		ib.barrier.subresourceRange.levelCount = mip_levels;
		ib.stages.second = vk::PipelineStageFlagBits::eAllCommands;
		ib.transition(cb);
	}
};

bool can_mip(vk::PhysicalDevice const gpu, vk::Format const format) {
	static constexpr vk::FormatFeatureFlags flags_v{vk::FormatFeatureFlagBits::eBlitDst | vk::FormatFeatureFlagBits::eBlitSrc};
	auto const fsrc = gpu.getFormatProperties(format);
	return (fsrc.optimalTilingFeatures & flags_v) != vk::FormatFeatureFlags{};
}

bool copy_to_image(VulkanDevice const& device, Vma::Image const& out, std::span<Image::View const> images, glm::ivec2 const offset = {}) {
	assert(std::all_of(images.begin(), images.end(), [images](Image::View const& view) { return view.extent == images[0].extent; }));
	assert(out.array_layers == images.size());
	auto const accumulate_size = [](std::size_t total, Image::View const& i) { return total + i.storage.size(); };
	auto const size = std::accumulate(images.begin(), images.end(), std::size_t{}, accumulate_size);
	auto staging = device.impl->vma.get().make_buffer(vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst, size, true);
	if (!staging.get().buffer) {
		// TODO: error
		return false;
	}

	auto* ptr = static_cast<std::byte*>(staging.get().ptr);
	for (auto const& image : images) {
		std::memcpy(ptr, image.storage.data(), image.storage.size());
		ptr += image.storage.size();
	}

	auto const vk_extent = vk::Extent3D{images[0].extent.x, images[0].extent.y, 1u};
	auto const vk_offset = vk::Offset3D{offset.x, offset.y, 0};
	auto isrl = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, out.array_layers);
	auto bic = vk::BufferImageCopy({}, {}, {}, isrl, vk_offset, vk_extent);
	auto cmd = Cmd{device};
	full_barrier(out, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal).transition(cmd.cb);
	cmd.cb.copyBufferToImage(staging.get().buffer, out.image, vk::ImageLayout::eTransferDstOptimal, bic);
	full_barrier(out, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal).transition(cmd.cb);
	if (out.mip_levels > 1) { MipMapWriter{out.image, out.extent, cmd.cb, out.mip_levels, out.array_layers}(); }
	return true;
}

UniqueImage make_image(VulkanDevice const& device, std::span<Image::View const> images, ImageCreateInfo const& info, vk::ImageViewType type) {
	if (images.empty()) { return {}; }
	auto const extent = vk::Extent2D{images[0].extent.x, images[0].extent.y};
	assert(std::all_of(images.begin(), images.end(), [images](Image::View const& view) { return view.extent == images[0].extent; }));
	auto ret = UniqueImage{device.impl->vma.get().make_image(info, extent, type)};
	if (!ret) { return {}; }
	copy_to_image(device, ret, images);
	return ret;
}

void copy_image(VulkanDevice const& device, CopyImage const& src, CopyImage const& dst, vk::Extent2D const extent) {
	auto isrl_src = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, src.image.array_layers);
	auto isrl_dst = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, src.image.array_layers);
	auto offset_src = vk::Offset3D{src.offset.x, src.offset.y, 0u};
	auto offset_dst = vk::Offset3D{dst.offset.x, dst.offset.y, 0u};
	auto ic = vk::ImageCopy{isrl_src, offset_src, isrl_dst, offset_dst, vk::Extent3D{extent, 1}};

	auto cmd = Cmd{device};
	full_barrier(src.image, src.layout, vk::ImageLayout::eTransferSrcOptimal).transition(cmd.cb);
	full_barrier(dst.image, dst.layout, vk::ImageLayout::eTransferDstOptimal).transition(cmd.cb);
	cmd.cb.copyImage(src.image.image, vk::ImageLayout::eTransferSrcOptimal, dst.image.image, vk::ImageLayout::eTransferDstOptimal, ic);
	auto layout = src.layout == vk::ImageLayout::eUndefined ? vk::ImageLayout::eShaderReadOnlyOptimal : src.layout;
	full_barrier(src.image, vk::ImageLayout::eTransferSrcOptimal, layout).transition(cmd.cb);
	layout = dst.layout == vk::ImageLayout::eUndefined ? vk::ImageLayout::eShaderReadOnlyOptimal : src.layout;
	full_barrier(dst.image, vk::ImageLayout::eTransferDstOptimal, layout).transition(cmd.cb);
	if (dst.image.mip_levels > 1) { MipMapWriter{dst.image.image, dst.image.extent, cmd.cb, dst.image.mip_levels, dst.image.array_layers}(); }
}
} // namespace

struct VulkanMeshGeometry::Impl::Uploader {
	VulkanDevice const& device;
	VulkanMeshGeometry::Impl& out;

	void operator()(Geometry::Packed const& geometry, Joints joints) {
		assert(joints.joints.size() == joints.weights.size());
		{
			auto staging = FlexArray<UniqueBuffer, 2>{};
			auto cmd = Cmd{device, vk::PipelineStageFlagBits::eTopOfPipe};
			staging.insert(upload(cmd.cb, geometry));
			if (!joints.joints.empty()) { staging.insert(upload(cmd.cb, joints.joints, joints.weights)); }
		}
		out.m_vlayout = joints.joints.empty() ? instanced_vertex_layout() : skinned_vertex_layout();
		out.m_instance_binding = 6u;
	}

	[[nodiscard]] UniqueBuffer upload(vk::CommandBuffer cb, Geometry::Packed const& geometry) {
		auto const indices = std::span<std::uint32_t const>{geometry.indices};
		out.m_vertices = static_cast<std::uint32_t>(geometry.positions.size());
		out.m_indices = static_cast<std::uint32_t>(indices.size());
		auto const size = geometry.size_bytes();
		out.m_vibo.swap(device.impl->vma.get().make_buffer(vi_flags_v, size, false));
		device.impl->vma.get().make_buffer(vi_flags_v, size, false);
		auto staging = device.impl->vma.get().make_buffer(vk::BufferUsageFlagBits::eTransferSrc, size, true);
		auto writer = Writer{staging};
		out.m_offsets.positions = writer(std::span{geometry.positions});
		out.m_offsets.rgbs = writer(std::span{geometry.rgbs});
		out.m_offsets.normals = writer(std::span{geometry.normals});
		out.m_offsets.uvs = writer(std::span{geometry.uvs});
		if (!indices.empty()) { out.m_offsets.indices = writer(indices); }
		cb.copyBuffer(staging.get().buffer, out.m_vibo.get().get().buffer, vk::BufferCopy{{}, {}, size});
		return staging;
	}

	[[nodiscard]] UniqueBuffer upload(vk::CommandBuffer cb, std::span<glm::uvec4 const> joints, std::span<glm::vec4 const> weights) {
		assert(joints.size() >= weights.size());
		auto const size = joints.size_bytes() + weights.size_bytes();
		out.m_jwbo.swap(device.impl->vma.get().make_buffer(v_flags_v, size, false));
		auto staging = device.impl->vma.get().make_buffer(vk::BufferUsageFlagBits::eTransferSrc, size, true);
		auto writer = Writer{staging};
		out.m_offsets.joints = writer(joints);
		out.m_offsets.weights = writer(weights);
		cb.copyBuffer(staging.get().buffer, out.m_jwbo.get().get().buffer, vk::BufferCopy{{}, {}, size});
		return staging;
	}
};

VulkanMeshGeometry::Impl::Impl(VulkanDevice const& device, Geometry::Packed const& geometry, Joints joints)
	: m_vibo(device.impl->defer), m_jwbo(device.impl->defer) {
	Uploader{device, *this}(geometry, joints);
}

VulkanTexture::Impl::Impl(VulkanDevice const& device, Image::View image, CreateInfo const& info) : Impl(device, info.sampler) {
	static constexpr auto magenta_pixmap_v = FixedPixelMap<1, 1>{{magenta_v}};
	m_info.format = info.colour_space == ColourSpace::eLinear ? vk::Format::eR8G8B8A8Unorm : vk::Format::eR8G8B8A8Srgb;
	bool mip_mapped = info.mip_mapped;
	if (image.extent.x == 0 || image.extent.y == 0) {
		logger::warn("[Texture] invalid image extent: [0x0]");
		image = magenta_pixmap_v.view();
		mip_mapped = false;
	}
	if (image.storage.empty()) {
		logger::warn("[Texture] invalid image bytes: [empty]");
		image = magenta_pixmap_v.view();
		mip_mapped = false;
	}
	if (mip_mapped && can_mip(device.impl->gpu.device, m_info.format)) { m_info.mip_levels = compute_mip_levels({image.extent.x, image.extent.y}); }
	m_image = {device.impl->defer, make_image(device, {&image, 1}, m_info, vk::ImageViewType::e2D)};
	m_layout = vk::ImageLayout::eShaderReadOnlyOptimal;
}

VulkanTexture::Impl::Impl(VulkanDevice const& device, TextureSampler sampler) : sampler(sampler), m_device(&device) {}

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
	ret.mip_levels = info.mip_levels;
	ret.array_layers = info.array_layers;
	ret.type = type;
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

RenderFrame RenderFrame::make(VulkanDevice const& device) {
	static constexpr auto flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
	auto ret = RenderFrame{
		.sync =
			Sync{
				.draw = device.impl->device->createSemaphoreUnique({}),
				.present = device.impl->device->createSemaphoreUnique({}),
				.drawn = device.impl->device->createFenceUnique({vk::FenceCreateFlagBits::eSignaled}),
			},
		.cmd_alloc = CmdAllocator::make(*device.impl->device, device.impl->queue_family, flags),
	};
	ret.primary = ret.cmd_alloc.allocate(false);
	for (auto& cmd : ret.secondary) { cmd = ret.cmd_alloc.allocate(true); }
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

VulkanDevice::VulkanDevice() : impl(std::make_unique<Impl>()) {}
VulkanDevice::VulkanDevice(VulkanDevice&&) noexcept = default;
VulkanDevice& VulkanDevice::operator=(VulkanDevice&&) noexcept = default;
VulkanDevice::~VulkanDevice() noexcept = default;

namespace {
void write_view(VulkanDevice const& device, Camera const& camera, Lights const& lights, glm::uvec2 const extent) {
	struct ViewSSBO {
		glm::mat4 mat_v;
		glm::mat4 mat_p;
		glm::vec4 vpos_exposure;
	} view{
		.mat_v = camera.view(),
		.mat_p = camera.projection(extent),
		.vpos_exposure = {camera.transform.position(), camera.exposure},
	};
	device.impl->view_ubo.update(&view, sizeof(view), 1);
	device.impl->view_ubo.flush(device.impl->vma, device.impl->buffered_index);

	struct DirLightSSBO {
		alignas(16) glm::vec3 direction{front_v};
		alignas(16) glm::vec3 ambient{0.04f};
		alignas(16) glm::vec3 diffuse{1.0f};

		static DirLightSSBO make(DirLight const& light) {
			return {
				.direction = glm::normalize(light.direction * front_v),
				.diffuse = light.rgb.to_vec4(),
			};
		}
	};
	if (!lights.dir_lights.empty()) {
		auto dir_lights = FlexArray<DirLightSSBO, max_lights_v>{};
		for (auto const& light : lights.dir_lights) {
			dir_lights.insert(DirLightSSBO::make(light));
			if (dir_lights.size() == dir_lights.capacity_v) { break; }
		}
		device.impl->dir_lights_ssbo.update(dir_lights.span().data(), dir_lights.span().size_bytes(), dir_lights.size());
		device.impl->dir_lights_ssbo.flush(device.impl->vma, device.impl->buffered_index);
	}
}

template <std::size_t Buffering>
void update_view(VulkanDevice const& device, VulkanShader<Buffering>& shader) {
	shader.update(0, 0, device.impl->view_ubo.view(device.impl->buffered_index));
	auto lights = device.impl->dir_lights_ssbo.view(device.impl->buffered_index);
	auto buffer = lights.buffer ? lights : device.impl->blank_buffer.get().view();
	shader.update(0, 1, buffer);
}

constexpr RenderMode combine(RenderMode const in, RenderMode def) {
	if (in.type != RenderMode::Type::eDefault) {
		def.type = in.type;
		def.line_width = in.line_width;
	}
	def.depth_test = in.depth_test;
	return def;
}

void draw_static(VulkanDevice const& device, StaticDrawInfo const& info, RenderCmd cmd, RenderPassView rp) {
	auto const& limits = device.impl->gpu.properties.limits;
	auto const* vmg = info.geometry.as<VulkanMeshGeometry>();
	assert(vmg);
	auto const& vlayout = vmg->impl->vertex_layout();
	auto const i = device.impl->buffered_index;
	auto vert = SpirV::from(info.providers.shader(), vlayout.shader_uri);
	auto frag = SpirV::from(info.providers.shader(), info.material.shader_id());
	if (!vert || !frag) { return; }
	auto rm = combine(info.material.render_mode(), device.impl->default_render_mode);
	auto const pipeline_state = PipelineState{
		.mode = from(rm.type),
		.topology = from(info.topology),
		.depth_test = rm.depth_test,
	};
	auto pipe = device.impl->pipelines.get(device.impl->vma, pipeline_state, vlayout.input, rp, {vert, frag}, i);
	auto shader = VulkanShader{pipe, device.impl->samplers};
	auto const line_width = std::clamp(rm.line_width, limits.lineWidthRange[0], limits.lineWidthRange[1]);
	pipe.bind(cmd.cb, cmd.extent, line_width);
	update_view(device, shader);
	info.material.write_sets(shader, info.providers.texture());
	shader.bind(pipe.layout, cmd.cb);
	assert(!info.geometry.has_joints());
	auto write_instances = [&](glm::mat4& out, std::size_t) { out = info.transform; };
	auto instance_buffer = device.impl->instance_vec.write(device.impl->vma, 1u, device.impl->buffered_index, write_instances);
	cmd.cb.bindVertexBuffers(vmg->impl->instance_binding(), instance_buffer.buffer, vk::DeviceSize{0});
	vmg->impl->draw(cmd.cb, 1u);
	++device.impl->stats.per_frame.draw_calls;
}

template <typename InfoT>
void render_mesh(VulkanDevice const& device, InfoT const& info, RenderCmd cmd, RenderPassView rp) {
	static Material const s_default_mat{std::make_unique<UnlitMaterial>()};
	auto const& limits = device.impl->gpu.properties.limits;
	for (auto const& primitive : info.mesh.primitives) {
		auto const* vmg = primitive.geometry.template as<VulkanMeshGeometry>();
		assert(vmg);
		if (!cmd.cb) {
			logger::error("[GraphicsDevice] Attempt to call draw outside active render pass");
			return;
		}
		auto const& material = info.providers.material().get(primitive.material, s_default_mat);
		auto rm = combine(material.render_mode(), device.impl->default_render_mode);
		auto const state = PipelineState{
			.mode = from(rm.type),
			.topology = from(primitive.topology),
			.depth_test = rm.depth_test,
		};
		auto cb = cmd.cb;
		auto const& vlayout = vmg->impl->vertex_layout();
		auto const i = device.impl->buffered_index;
		auto vert = SpirV::from(info.providers.shader(), vlayout.shader_uri);
		auto frag = SpirV::from(info.providers.shader(), material.shader_id());
		if (!vert || !frag) { return; }
		auto pipe = device.impl->pipelines.get(device.impl->vma, state, vlayout.input, rp, {vert, frag}, i);
		auto shader = VulkanShader{pipe, device.impl->samplers};
		auto const line_width = std::clamp(rm.line_width, limits.lineWidthRange[0], limits.lineWidthRange[1]);
		pipe.bind(cb, cmd.extent, line_width);
		update_view(device, shader);
		material.write_sets(shader, info.providers.texture());
		if constexpr (std::same_as<InfoT, SkinnedMeshRenderInfo>) {
			assert(primitive.geometry.has_joints());
			auto joints_set = vmg->impl->joints_set();
			assert(joints_set);
			assert(info.mesh.inverse_bind_matrices.size() >= info.joints.size());
			auto rewrite = [&](glm::mat4& out, std::size_t index) { out = info.joints[index] * info.mesh.inverse_bind_matrices[index]; };
			auto const joint_mats = [&] { return device.impl->joints_vec.write(device.impl->vma, info.joints.size(), device.impl->buffered_index, rewrite); }();
			shader.update(*joints_set, 0, joint_mats);
			shader.bind(pipe.layout, cb);
			vmg->impl->draw(cb);
		} else {
			shader.bind(pipe.layout, cb);
			assert(!primitive.geometry.has_joints());
			auto write_instances = [&](glm::mat4& out, std::size_t index) { out = info.parent * info.instances[index].matrix(); };
			auto instance_buffer = device.impl->instance_vec.write(device.impl->vma, info.instances.size(), device.impl->buffered_index, write_instances);
			cb.bindVertexBuffers(vmg->impl->instance_binding(), instance_buffer.buffer, vk::DeviceSize{0});
			vmg->impl->draw(cb, static_cast<std::uint32_t>(info.instances.size()));
		}
		++device.impl->stats.per_frame.draw_calls;
	}
}

struct VulkanRenderPass : RenderPass {
	VulkanDevice const& device;
	RenderCmd cmd{};
	RenderPassView rpv{};

	VulkanRenderPass(VulkanDevice const& device, AssetProviders const& providers, RenderCmd cmd, RenderPassView rpv)
		: RenderPass(providers), device(device), cmd(cmd), rpv(rpv) {}

	void render(StaticMesh const& mesh, std::span<Transform const> instances, glm::mat4 const& parent) const final {
		auto const smri = StaticMeshRenderInfo{
			m_providers,
			mesh,
			parent,
			instances,
		};
		render_mesh(device, smri, cmd, rpv);
	}

	void render(SkinnedMesh const& mesh, std::span<glm::mat4 const> joints) const final {
		auto const smri = SkinnedMeshRenderInfo{
			m_providers,
			mesh,
			joints,
		};
		render_mesh(device, smri, cmd, rpv);
	}

	void draw(MeshGeometry const& geometry, Material const& material, glm::mat4 const& transform, Topology topology) const final {
		auto sdi = StaticDrawInfo{
			m_providers, geometry, material, transform, topology,
		};
		draw_static(device, sdi, cmd, rpv);
	}
};
} // namespace
} // namespace levk

void levk::gfx_create_device(VulkanDevice& out, GraphicsDeviceCreateInfo const& create_info) {
	auto const extensions = create_info.window.vulkan_extensions();
	out.impl->instance = make_instance({extensions.begin(), extensions.end()}, create_info, out.impl->info);
	if (create_info.validation) { out.impl->debug = make_debug_messenger(*out.impl->instance); }
	auto surface = VulkanSurface{};
	auto instance = VulkanSurface::Instance{};
	instance.instance = *out.impl->instance;
	create_info.window.make_surface(surface, instance);
	out.impl->surface = std::move(surface.surface);
	out.impl->gpu = select_gpu(*out.impl->instance, *out.impl->surface);
	auto layers = FlexArray<char const*, 2>{};
	if (create_info.validation) { layers.insert(validation_layer_v.data()); }
	out.impl->device = make_device(layers.span(), out.impl->gpu);
	out.impl->queue = out.impl->device->getQueue(out.impl->gpu.queue_family, 0);

	out.impl->vma = make_vma(*out.impl->instance, out.impl->gpu.device, *out.impl->device);

	out.impl->swapchain.formats = make_swapchain_formats(out.impl->gpu.device.getSurfaceFormatsKHR(*out.impl->surface));
	auto const supported_modes = out.impl->gpu.device.getSurfacePresentModesKHR(*out.impl->surface);
	out.impl->swapchain.modes = {supported_modes.begin(), supported_modes.end()};
	auto const present_mode = ideal_present_mode(out.impl->swapchain.modes, from(create_info.vsync));
	out.impl->swapchain.info = make_swci(out.impl->queue_family, *out.impl->surface, out.impl->swapchain.formats, create_info.swapchain, present_mode);
	out.impl->info.swapchain = is_srgb(out.impl->swapchain.info.imageFormat) ? ColourSpace::eSrgb : ColourSpace::eLinear;
	out.impl->info.supported_vsync = make_vsync(out.impl->gpu.device, *out.impl->surface);
	out.impl->info.current_vsync = from(out.impl->swapchain.info.presentMode);
	out.impl->info.supported_aa = make_aa(out.impl->gpu.properties);
	out.impl->info.current_aa = get_samples(out.impl->info.supported_aa, create_info.anti_aliasing);
	out.impl->info.validation = create_info.validation;
	out.impl->info.name = out.impl->gpu.properties.deviceName;

	auto const depth = depth_format(out.impl->gpu.device);
	auto const samples = from(out.impl->info.current_aa);
	out.impl->off_screen = OffScreen<>::make(out, samples, depth);
	out.impl->fs_quad.rp = make_fs_quad_pass(out, out.impl->swapchain.info.imageFormat);

	out.impl->dear_imgui = make_imgui(create_info.window, out);
	out.impl->dear_imgui.new_frame();

	out.impl->pipelines.sample_rate_shading = out.impl->gpu.device.getFeatures().sampleRateShading;

	out.impl->samplers.anisotropy = out.impl->gpu.properties.limits.maxSamplerAnisotropy;

	out.impl->instance_vec = {vk::BufferUsageFlagBits::eVertexBuffer};
	out.impl->joints_vec = {vk::BufferUsageFlagBits::eStorageBuffer};
	out.impl->blank_buffer = out.impl->vma.get().make_buffer(vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eStorageBuffer, 1u, true);
	static constexpr auto blank_byte_v = std::byte{};
	std::memcpy(out.impl->blank_buffer.get().ptr, &blank_byte_v, 1u);

	out.impl->view_ubo = {vk::BufferUsageFlagBits::eUniformBuffer};
	out.impl->dir_lights_ssbo = {vk::BufferUsageFlagBits::eStorageBuffer};
}

void levk::gfx_destroy_device(VulkanDevice& out) {
	out.impl->device->waitIdle();
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

levk::GraphicsDeviceInfo const& levk::gfx_info(VulkanDevice const& device) { return device.impl->info; }

bool levk::gfx_set_vsync(VulkanDevice& out, Vsync::Type vsync) {
	if (make_swapchain(out, {}, from(vsync)) == vk::Result::eSuccess) {
		out.impl->info.current_vsync = vsync;
		return true;
	}
	return false;
}

bool levk::gfx_set_render_scale(VulkanDevice& out, float scale) {
	if (scale < render_scale_limit_v[0] || scale > render_scale_limit_v[1]) { return false; }
	out.impl->info.render_scale = scale;
	return true;
}

void levk::gfx_render(VulkanDevice const& device, RenderInfo const& info) {
	struct OnReturn {
		VulkanDevice const& out;
		Index<> i;
		OnReturn(VulkanDevice const& out) : out(out), i(out.impl->buffered_index) {}

		~OnReturn() {
			out.impl->buffered_index.next();
			out.impl->dear_imgui.new_frame();
			out.impl->defer.next();
			out.impl->instance_vec.release();
			out.impl->joints_vec.release();
			out.impl->pipelines.release_all(i);
		}
	};

	device.impl->stats.per_frame = {};

	auto const on_return = OnReturn{device};
	if (info.extent.x == 0 || info.extent.y == 0) { return; }

	auto& frame = device.impl->off_screen.rp.frames.get(device.impl->buffered_index);

	auto recreate = [&device, extent = info.extent] {
		if (make_swapchain(device, extent, {}) == vk::Result::eSuccess) {
			device.impl->swapchain.storage.extent = extent;
			return true;
		}
		return false;
	};

	if (info.extent != device.impl->swapchain.storage.extent && !recreate()) { return; }

	struct {
		ImageView image;
		std::uint32_t index;
	} acquired{};
	auto result = vk::Result::eErrorOutOfDateKHR;
	{
		auto lock = std::scoped_lock{device.impl->mutex};
		// acquire swapchain image
		result = device.impl->device->acquireNextImageKHR(*device.impl->swapchain.storage.swapchain, std::numeric_limits<std::uint64_t>::max(),
														  *frame.sync.draw, {}, &acquired.index);
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
	auto const images = device.impl->swapchain.storage.images.span();
	assert(acquired.index < images.size());
	acquired.image = images[acquired.index];
	device.impl->swapchain.storage.extent = info.extent;

	if (!acquired.image.image) { return; }

	reset(*device.impl->device, *frame.sync.drawn);

	// write view
	write_view(device, info.camera, info.lights, info.extent);

	// refresh render targets
	auto& fs_quad_fb = device.impl->fs_quad.framebuffer.get(device.impl->buffered_index);
	frame.framebuffer = device.impl->off_screen.make_framebuffer(device, acquired.image.extent, device.impl->info.render_scale);
	fs_quad_fb = device.impl->fs_quad.make_framebuffer(device, acquired.image);

	// begin recording
	auto cmd_3d = RenderCmd{frame.secondary[0], frame.framebuffer.render_target.extent};
	auto cmd_ui = RenderCmd{frame.secondary[1], fs_quad_fb.render_target.extent};
	auto cbii = vk::CommandBufferInheritanceInfo{*device.impl->off_screen.rp.render_pass, 0, *frame.framebuffer.framebuffer};
	cmd_3d.cb.begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue, &cbii});
	cbii = {*device.impl->fs_quad.rp, 0, *fs_quad_fb.framebuffer};
	cmd_ui.cb.begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue, &cbii});

	device.impl->default_render_mode = info.default_render_mode;
	// dispatch 3D render
	info.renderer.render_3d(VulkanRenderPass{device, info.providers, cmd_3d, device.impl->off_screen.view()});

	// trace 3D output image to native render pass
	auto fs_quad_vert = SpirV::from(info.providers.shader(), "shaders/fs_quad.vert");
	auto fs_quad_frag = SpirV::from(info.providers.shader(), "shaders/fs_quad.frag");
	auto fs_quad_pipe = device.impl->pipelines.get(device.impl->vma, {}, {}, {*device.impl->fs_quad.rp, vk::SampleCountFlagBits::e1},
												   {fs_quad_vert, fs_quad_frag}, device.impl->buffered_index);
	// bind 3D output image to 0, 0
	auto& fs_quad_set0 = fs_quad_pipe.set_pool->next_set(0);
	fs_quad_set0.update(0, frame.framebuffer.render_target.output_image(), device.impl->samplers.get(*device.impl->device, {}));
	fs_quad_pipe.bind(cmd_ui.cb, fs_quad_fb.render_target.extent);
	cmd_ui.cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, fs_quad_pipe.layout, 0u, fs_quad_set0.set(), {});
	// draw quad hard-coded in shader
	cmd_ui.cb.draw(6u, 1u, {}, {});

	// dispatch UI render
	info.renderer.render_ui(VulkanRenderPass{device, info.providers, cmd_ui, device.impl->fs_quad.view()});

	// dispatch Dear ImGui render
	device.impl->dear_imgui.end_frame();
	device.impl->dear_imgui.render(cmd_ui.cb);

	// end recording
	cmd_3d.cb.end();
	cmd_ui.cb.end();

	// begin rendering
	frame.primary.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

	auto const clear_colour = info.clear.to_vec4();
	auto const vk_clear_colour = vk::ClearColorValue{std::array{clear_colour.x, clear_colour.y, clear_colour.z, clear_colour.w}};
	// execute 3D render pass
	vk::ClearValue const clear_values[] = {vk_clear_colour, vk::ClearDepthStencilValue{1.0f, 0u}};
	auto rpbi = vk::RenderPassBeginInfo{*device.impl->off_screen.rp.render_pass, *frame.framebuffer.framebuffer,
										vk::Rect2D{{}, frame.framebuffer.render_target.extent}, clear_values};
	frame.primary.beginRenderPass(rpbi, vk::SubpassContents::eSecondaryCommandBuffers);
	frame.primary.executeCommands(1u, &cmd_3d.cb);
	frame.primary.endRenderPass();
	// execute UI render pass
	rpbi = vk::RenderPassBeginInfo{*device.impl->fs_quad.rp, *fs_quad_fb.framebuffer, vk::Rect2D{{}, fs_quad_fb.render_target.extent}, clear_values};
	frame.primary.beginRenderPass(rpbi, vk::SubpassContents::eSecondaryCommandBuffers);
	frame.primary.executeCommands(1u, &cmd_ui.cb);
	frame.primary.endRenderPass();

	// end rendering
	frame.primary.end();

	static constexpr vk::PipelineStageFlags submit_wait = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	auto submit_info = vk::SubmitInfo{*frame.sync.draw, submit_wait, frame.primary, *frame.sync.present};

	auto present_info = vk::PresentInfoKHR{};
	present_info.swapchainCount = 1u;
	present_info.pSwapchains = &*device.impl->swapchain.storage.swapchain;
	present_info.waitSemaphoreCount = 1u;
	present_info.pWaitSemaphores = &*frame.sync.present;
	present_info.pImageIndices = &acquired.index;

	{
		auto lock = std::scoped_lock{device.impl->mutex};
		// submit commands
		device.impl->queue.submit(submit_info, *frame.sync.drawn);
		// present image
		result = device.impl->queue.presentKHR(&present_info);
	}
}

levk::MeshGeometry levk::gfx_make_mesh_geometry(VulkanDevice const& device, Geometry::Packed const& geometry, MeshJoints const& joints) {
	auto mesh = VulkanMeshGeometry::Impl{device, geometry, joints};
	auto ret = VulkanMeshGeometry{};
	ret.impl = std::make_unique<VulkanMeshGeometry::Impl>(VulkanMeshGeometry::Impl{std::move(mesh)});
	return ret;
}

std::uint32_t levk::gfx_mesh_vertex_count(VulkanMeshGeometry const& mesh) { return mesh.impl->info().vertices; }
std::uint32_t levk::gfx_mesh_index_count(VulkanMeshGeometry const& mesh) { return mesh.impl->info().indices; }
bool levk::gfx_mesh_has_joints(VulkanMeshGeometry const& mesh) { return mesh.impl->has_joints(); }

levk::Texture levk::gfx_make_texture(VulkanDevice const& device, Texture::CreateInfo create_info, Image::View image) {
	auto ret = VulkanTexture{};
	ret.impl = std::make_unique<VulkanTexture::Impl>(device, image, create_info);
	return Texture{std::move(ret), std::move(create_info.name)};
}

levk::TextureSampler const& levk::gfx_tex_sampler(VulkanTexture const& texture) { return texture.impl->sampler; }
levk::ColourSpace levk::gfx_tex_colour_space(VulkanTexture const& texture) { return texture.impl->colour_space(); }

levk::Extent2D levk::gfx_tex_extent(VulkanTexture const& texture) {
	auto const view = texture.impl->view();
	return {view.extent.width, view.extent.height};
}

std::uint32_t levk::gfx_tex_mip_levels(VulkanTexture const& texture) { return texture.impl->mip_levels(); }

bool levk::gfx_tex_resize_canvas(VulkanTexture& texture, Extent2D new_extent, Rgba background, glm::uvec2 top_left) {
	if (new_extent.x == 0 || new_extent.y == 0) { return false; }
	auto const extent = gfx_tex_extent(texture);
	if (new_extent == extent) { return true; }
	if (top_left.x + extent.x > new_extent.x || top_left.y + extent.y > new_extent.y) { return false; }
	// TODO: cubemaps
	auto const image_type = texture.impl->m_image.get().get().type;
	assert(image_type == vk::ImageViewType::e2D);
	auto pixels = DynPixelMap{new_extent};
	for (Rgba& rgba : pixels.span()) { rgba = background; }
	Image::View image_view[] = {pixels.view()};
	auto new_image = make_image(*texture.impl->m_device, image_view, texture.impl->m_info, image_type);
	auto src = CopyImage{texture.impl->m_image.get().get(), texture.impl->m_layout};
	auto dst = CopyImage{new_image.get(), texture.impl->m_layout, top_left};
	copy_image(*texture.impl->m_device, src, dst, texture.impl->view().extent);
	texture.impl->m_image = {texture.impl->m_device->impl->defer, std::move(new_image)};
	return true;
}

bool levk::gfx_tex_write(VulkanTexture& texture, Image::View image, glm::uvec2 offset) {
	auto const bottom_right = offset + image.extent;
	auto const texture_extent = gfx_tex_extent(texture);
	if (bottom_right.x > texture_extent.x || bottom_right.y > texture_extent.y) { return false; }
	return copy_to_image(*texture.impl->m_device, texture.impl->m_image.get().get(), {&image, 1}, offset);
}

levk::RenderStats levk::gfx_render_stats(VulkanDevice const& device) {
	return {
		device.impl->stats.per_frame.draw_calls,
	};
}
