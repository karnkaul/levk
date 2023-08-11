#include <glm/mat4x4.hpp>
#include <graphics/vulkan/ad_hoc_cmd.hpp>
#include <graphics/vulkan/common.hpp>
#include <graphics/vulkan/image_barrier.hpp>
#include <levk/graphics/geometry.hpp>
#include <levk/util/error.hpp>
#include <levk/util/hash_combine.hpp>
#include <cmath>
#include <numeric>

namespace levk::vulkan {
namespace {
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

constexpr vk::BorderColor from(TextureSampler::Border const border) {
	switch (border) {
	default:
	case TextureSampler::Border::eOpaqueBlack: return vk::BorderColor::eFloatOpaqueBlack;
	case TextureSampler::Border::eOpaqueWhite: return vk::BorderColor::eFloatOpaqueWhite;
	case TextureSampler::Border::eTransparentBlack: return vk::BorderColor::eFloatTransparentBlack;
	}
}

struct MipMapWriter {
	ImageBarrier& ib;

	vk::Extent2D extent;
	vk::CommandBuffer cb;
	std::uint32_t mip_levels;

	std::uint32_t layer_count{1};

	void blit_mips(std::uint32_t const src_level, vk::Offset3D const src_offset, vk::Offset3D const dst_offset) const {
		auto const src_isrl = vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, src_level, 0, layer_count};
		auto const dst_isrl = vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, src_level + 1, 0, layer_count};
		auto const region = vk::ImageBlit{src_isrl, {vk::Offset3D{}, src_offset}, dst_isrl, {vk::Offset3D{}, dst_offset}};
		cb.blitImage(ib.barrier.image, vk::ImageLayout::eTransferSrcOptimal, ib.barrier.image, vk::ImageLayout::eTransferDstOptimal, region,
					 vk::Filter::eLinear);
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
		ib.barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		ib.barrier.subresourceRange.baseArrayLayer = 0;
		ib.barrier.subresourceRange.layerCount = layer_count;
		ib.barrier.subresourceRange.baseMipLevel = 0;
		ib.barrier.subresourceRange.levelCount = 1;

		ib.barrier.srcAccessMask = ib.barrier.dstAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite;
		ib.barrier.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands;
		ib.barrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;

		ib.barrier.oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		ib.barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
		ib.barrier.subresourceRange.baseMipLevel = 0;
		ib.transition(cb);

		ib.barrier.srcStageMask = ib.barrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;

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
		ib.barrier.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands;
		ib.transition(cb);
	}
};
} // namespace

void Vma::Deleter::operator()(Vma const& vma) const {
	if (vma.allocator) { vmaDestroyAllocator(vma.allocator); }
}

void Vma::Deleter::operator()(Buffer const& buffer) const {
	auto const& allocation = buffer.allocation;
	if (buffer.buffer && allocation.allocation) { vmaDestroyBuffer(allocation.vma.allocator, buffer.buffer, allocation.allocation); }
}

void Vma::Deleter::operator()(Image const& image) const {
	auto const& allocation = image.allocation;
	if (image.image && allocation.allocation) { vmaDestroyImage(allocation.vma.allocator, image.image, allocation.allocation); }
}

UniqueVma Vma::make(vk::Instance instance, vk::PhysicalDevice gpu, vk::Device device) {
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

UniqueBuffer Vma::make_buffer(vk::BufferUsageFlags const usage, vk::DeviceSize const size, bool host_visible) const {
	auto vaci = VmaAllocationCreateInfo{};
	vaci.usage = VMA_MEMORY_USAGE_AUTO;
	if (host_visible) { vaci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT; }
	auto const bci = vk::BufferCreateInfo{{}, size, usage};
	auto vbci = static_cast<VkBufferCreateInfo>(bci);
	auto buffer = VkBuffer{};
	auto ret = Buffer{};
	auto alloc_info = VmaAllocationInfo{};
	if (vmaCreateBuffer(allocator, &vbci, &vaci, &buffer, &ret.allocation.allocation, &alloc_info) != VK_SUCCESS) {
		throw Error{"Failed to allocate Vulkan Buffer"};
	}
	ret.buffer = buffer;
	ret.allocation.vma = *this;
	ret.size = bci.size;
	ret.mapped = alloc_info.pMappedData;
	if (host_visible) { assert(ret.mapped); }
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
	ret.format = info.format;
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

void Vma::copy_image(vk::CommandBuffer cb, Copy const& src, Copy const& dst, vk::Extent2D const extent) const {
	auto isrl_src = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, src.image.array_layers);
	auto isrl_dst = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, src.image.array_layers);
	auto offset_src = vk::Offset3D{src.offset.x, src.offset.y, 0u};
	auto offset_dst = vk::Offset3D{dst.offset.x, dst.offset.y, 0u};
	auto ic = vk::ImageCopy{isrl_src, offset_src, isrl_dst, offset_dst, vk::Extent3D{extent, 1}};

	auto barriers = std::array<vk::ImageMemoryBarrier2, 2>{};
	barriers[0] = ImageBarrier{src.image}.set_full_barrier(src.layout, vk::ImageLayout::eTransferSrcOptimal).barrier;
	barriers[1] = ImageBarrier{dst.image}.set_full_barrier(dst.layout, vk::ImageLayout::eTransferDstOptimal).barrier;
	ImageBarrier::transition(cb, barriers);

	cb.copyImage(src.image.image, vk::ImageLayout::eTransferSrcOptimal, dst.image.image, vk::ImageLayout::eTransferDstOptimal, ic);

	auto layout = src.layout == vk::ImageLayout::eUndefined ? vk::ImageLayout::eShaderReadOnlyOptimal : src.layout;
	barriers[0] = ImageBarrier{src.image}.set_full_barrier(vk::ImageLayout::eTransferSrcOptimal, layout).barrier;
	layout = dst.layout == vk::ImageLayout::eUndefined ? vk::ImageLayout::eShaderReadOnlyOptimal : dst.layout;
	barriers[1] = ImageBarrier{dst.image}.set_full_barrier(vk::ImageLayout::eTransferDstOptimal, layout).barrier;
	ImageBarrier::transition(cb, barriers);

	if (dst.image.mip_levels > 1) {
		auto barrier = ImageBarrier{dst.image};
		MipMapWriter{barrier, dst.image.extent, cb, dst.image.mip_levels, dst.image.array_layers}();
	}
}

UniqueBuffer Vma::copy_to_image(vk::CommandBuffer cb, Image const& out, std::span<levk::Image::View const> images, glm::ivec2 const offset) const {
	assert(std::all_of(images.begin(), images.end(), [images](levk::Image::View const& view) { return view.extent == images[0].extent; }));
	assert(out.array_layers == images.size());
	auto const accumulate_size = [](std::size_t total, levk::Image::View const& i) { return total + i.storage.size(); };
	auto const size = std::accumulate(images.begin(), images.end(), std::size_t{}, accumulate_size);
	auto staging = make_buffer(vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst, size, true);
	if (!staging.get().buffer || !staging.get().mapped) { throw Error{"Failed to write create Vulkan staging buffer"}; }

	auto* ptr = static_cast<std::byte*>(staging.get().mapped);
	for (auto const& image : images) {
		std::memcpy(ptr, image.storage.data(), image.storage.size());
		ptr += image.storage.size();
	}

	auto const vk_extent = vk::Extent3D{images[0].extent.x, images[0].extent.y, 1u};
	auto const vk_offset = vk::Offset3D{offset.x, offset.y, 0};
	auto isrl = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, out.array_layers);
	auto bic = vk::BufferImageCopy({}, {}, {}, isrl, vk_offset, vk_extent);
	auto barrier = ImageBarrier{out};
	barrier.set_full_barrier(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal).transition(cb);
	cb.copyBufferToImage(staging.get().buffer, out.image, vk::ImageLayout::eTransferDstOptimal, bic);
	barrier.set_full_barrier(vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal).transition(cb);
	if (out.mip_levels > 1) { MipMapWriter{barrier, out.extent, cb, out.mip_levels, out.array_layers}(); }

	return staging;
}

UniqueBuffer Vma::write_images(vk::CommandBuffer cb, Image const& out, std::span<ImageWrite const> writes) const {
	if (out.array_layers > 1u || writes.empty()) { return {}; }
	auto const accumulate_size = [](std::size_t total, ImageWrite const& tw) { return total + tw.image.storage.size(); };
	auto const size = std::accumulate(writes.begin(), writes.end(), std::size_t{}, accumulate_size);
	auto staging = make_buffer(vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst, size, true);
	if (!staging.get().buffer || !staging.get().mapped) { throw Error{"Failed to write create Vulkan staging buffer"}; }

	auto* ptr = static_cast<std::byte*>(staging.get().mapped);
	for (auto const& image : writes) {
		std::memcpy(ptr, image.image.storage.data(), image.image.storage.size());
		ptr += image.image.storage.size();
	}

	auto isrl = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, out.array_layers);
	auto bics = std::vector<vk::BufferImageCopy>{};
	bics.reserve(writes.size());
	auto buffer_offset = vk::DeviceSize{};
	for (auto const& image : writes) {
		auto offset = glm::ivec2{image.offset};
		auto const vk_offset = vk::Offset3D{offset.x, offset.y, 0};
		auto const extent = vk::Extent3D{image.image.extent.x, image.image.extent.y, 1u};
		auto bic = vk::BufferImageCopy{buffer_offset, {}, {}, isrl, vk_offset, extent};
		bics.push_back(bic);
		buffer_offset += image.image.storage.size();
	}
	auto barrier = ImageBarrier{out};
	barrier.set_full_barrier(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal).transition(cb);
	cb.copyBufferToImage(staging.get().buffer, out.image, vk::ImageLayout::eTransferDstOptimal, bics);
	barrier.set_full_barrier(vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal).transition(cb);
	if (out.mip_levels > 1) { MipMapWriter{barrier, out.extent, cb, out.mip_levels, out.array_layers}(); }
	return staging;
}

void Vma::full_blit(vk::CommandBuffer cb, Blit src, Blit dst, vk::Filter filter) const {
	static constexpr auto isrl_v = vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, {}, {}, 1u};
	auto const extent_src = glm::ivec2{src.image.extent.width, src.image.extent.height};
	auto const offsets_src = std::array<vk::Offset3D, 2>{vk::Offset3D{}, vk::Offset3D{extent_src.x, extent_src.y, 1u}};
	auto const extent_dst = glm::ivec2{dst.image.extent.width, dst.image.extent.height};
	auto const offsets_dst = std::array<vk::Offset3D, 2>{vk::Offset3D{}, vk::Offset3D{extent_dst.x, extent_dst.y, 1u}};
	auto const ib = vk::ImageBlit{isrl_v, offsets_src, isrl_v, offsets_dst};
	cb.blitImage(src.image.image, vk::ImageLayout::eTransferSrcOptimal, dst.image.image, vk::ImageLayout::eTransferDstOptimal, ib, filter);
}

void Queue::make(Queue& out, vk::Device device, std::uint32_t family, std::uint32_t index) {
	out.family = family;
	out.index = index;
	out.queue = device.getQueue(family, index);
}

vk::UniqueDescriptorPool SetAllocator::make_descriptor_pool(vk::Device device, std::uint32_t max_sets) {
	static constexpr auto pool_sizes = std::array{
		vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 4},
		vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, 4},
		vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 4},
	};
	if (pool_sizes.empty()) { return {}; }
	return device.createDescriptorPoolUnique({{}, max_sets, pool_sizes});
}

vk::DescriptorSet SetAllocator::allocate(vk::DescriptorSetLayout layout) {
	auto ret = vk::DescriptorSet{};
	for (int loops{}; loops < 128 && !ret; ++loops) {
		auto pool = get_free_pool(layout);
		if (!pool) { throw Error{"Failed to allocate descriptor pool"}; }
		ret = try_allocate(pool, layout);
	}
	if (!ret) { throw Error{"Failed to allocate descriptor set"}; }
	return ret;
}

void SetAllocator::reset_all() {
	for (auto& [_, page] : pools.pages) {
		for (auto& pool : page.values) { device.resetDescriptorPool(*pool); }
		page.next_free = {};
	}
}

vk::DescriptorPool SetAllocator::get_free_pool(vk::DescriptorSetLayout layout) {
	auto& ret = pools.next(layout);
	if (!ret) { ret = make_descriptor_pool(device); }
	return *ret;
}

vk::DescriptorSet SetAllocator::try_allocate(vk::DescriptorPool pool, vk::DescriptorSetLayout layout) const {
	auto dsai = vk::DescriptorSetAllocateInfo{pool, layout};
	dsai.descriptorSetCount = 1u;
	auto ret = vk::DescriptorSet{};
	if (device.allocateDescriptorSets(&dsai, &ret) != vk::Result::eSuccess) { return {}; }
	return ret;
}

std::size_t VertexInput::make_hash() const {
	hash = std::size_t{};
	for (auto const& binding : bindings.span()) { hash_combine(hash, binding.binding, binding.stride, binding.inputRate); }
	for (auto const& attribute : attributes.span()) { hash_combine(hash, attribute.binding, attribute.format, attribute.location, attribute.offset); }
	return hash;
}

VertexInput VertexInput::for_shadow() {
	auto ret = VertexInput{};

	// position
	ret.bindings.insert(vk::VertexInputBindingDescription{0, sizeof(glm::vec3)});
	ret.attributes.insert(vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32B32Sfloat});

	// instance matrix
	ret.bindings.insert(vk::VertexInputBindingDescription{4, sizeof(glm::mat4), vk::VertexInputRate::eInstance});
	ret.attributes.insert(vk::VertexInputAttributeDescription{4, 4, vk::Format::eR32G32B32A32Sfloat, 0 * sizeof(glm::vec4)});
	ret.attributes.insert(vk::VertexInputAttributeDescription{5, 4, vk::Format::eR32G32B32A32Sfloat, 1 * sizeof(glm::vec4)});
	ret.attributes.insert(vk::VertexInputAttributeDescription{6, 4, vk::Format::eR32G32B32A32Sfloat, 2 * sizeof(glm::vec4)});
	ret.attributes.insert(vk::VertexInputAttributeDescription{7, 4, vk::Format::eR32G32B32A32Sfloat, 3 * sizeof(glm::vec4)});

	return ret;
}

VertexInput VertexInput::for_static() {
	auto ret = for_shadow();

	// rgb
	ret.bindings.insert(vk::VertexInputBindingDescription{1, sizeof(glm::vec4)});
	ret.attributes.insert(vk::VertexInputAttributeDescription{1, 1, vk::Format::eR32G32B32A32Sfloat});

	// normal
	ret.bindings.insert(vk::VertexInputBindingDescription{2, sizeof(glm::vec3)});
	ret.attributes.insert(vk::VertexInputAttributeDescription{2, 2, vk::Format::eR32G32B32Sfloat});

	// uv
	ret.bindings.insert(vk::VertexInputBindingDescription{3, sizeof(glm::vec2)});
	ret.attributes.insert(vk::VertexInputAttributeDescription{3, 3, vk::Format::eR32G32Sfloat});

	return ret;
}

VertexInput VertexInput::for_skinned() {
	auto ret = for_static();

	// joints
	ret.bindings.insert(vk::VertexInputBindingDescription{8, sizeof(glm::uvec4)});
	ret.attributes.insert(vk::VertexInputAttributeDescription{8, 8, vk::Format::eR32G32B32A32Uint});

	// weights
	ret.bindings.insert(vk::VertexInputBindingDescription{9, sizeof(glm::vec4)});
	ret.attributes.insert(vk::VertexInputAttributeDescription{9, 9, vk::Format::eR32G32B32A32Sfloat});

	return ret;
}

std::size_t SamplerStorage::Hasher::operator()(TextureSampler const& sampler) const {
	return make_combined_hash(sampler.min, sampler.mag, sampler.wrap_s, sampler.wrap_t, sampler.border);
}

vk::Sampler SamplerStorage::get(vk::Device device, TextureSampler const& sampler) {
	if (auto it = map.find(sampler); it != map.end()) { return *it->second; }
	auto sci = vk::SamplerCreateInfo{};
	sci.minFilter = from(sampler.min);
	sci.magFilter = from(sampler.mag);
	sci.anisotropyEnable = anisotropy > 0.0f;
	sci.maxAnisotropy = anisotropy;
	sci.borderColor = from(sampler.border);
	sci.mipmapMode = vk::SamplerMipmapMode::eNearest;
	sci.addressModeU = from(sampler.wrap_s);
	sci.addressModeV = from(sampler.wrap_t);
	sci.addressModeW = from(sampler.wrap_s);
	sci.maxLod = VK_LOD_CLAMP_NONE;
	auto [it, _] = map.insert_or_assign(sampler, device.createSamplerUnique(sci));
	return *it->second;
}

std::uint32_t DeviceView::compute_mip_levels(vk::Extent2D extent) {
	return static_cast<std::uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1u;
}

DeviceBuffer DeviceBuffer::make(DeviceView device, vk::BufferUsageFlags usage) {
	return DeviceBuffer{
		.buffer = *device.defer,
		.usage = usage,
		.device = device,
	};
}

void DeviceBuffer::write(void const* data, std::size_t size, std::size_t count) {
	auto staging = device.vma.make_buffer(vk::BufferUsageFlagBits::eTransferSrc, size, true);
	if (buffer.get().get().size < size) { buffer.get() = device.vma.make_buffer(vk::BufferUsageFlagBits::eTransferDst | usage, size, false); }
	if (!staging.get().buffer || !staging.get().mapped) { throw Error{"Failed to write create Vulkan staging buffer"}; }
	std::memcpy(staging.get().mapped, data, size);
	auto cmd = AdHocCmd{device};
	cmd.cb.copyBuffer(staging.get().buffer, buffer.get().get().buffer, vk::BufferCopy{{}, {}, size});
	this->count = static_cast<std::uint32_t>(count);
}

BufferView DeviceBuffer::view() const {
	auto ret = buffer.get().get().view();
	ret.count = count;
	return ret;
}

HostBuffer HostBuffer::make(DeviceView device, vk::BufferUsageFlags usage) {
	return HostBuffer{
		.buffers = *device.defer,
		.usage = usage,
		.device = device,
	};
}

void HostBuffer::write(void const* data, std::size_t size, std::size_t count) {
	bytes.resize(size);
	std::memcpy(bytes.data(), data, size);
	this->count = static_cast<std::uint32_t>(count);
}

BufferView HostBuffer::view() {
	if (bytes.empty()) { return {}; }
	auto& ret = buffers.get()[*device.buffered_index];
	if (ret.get().size < bytes.size()) { ret = device.vma.make_buffer(usage, bytes.size(), true); }
	if (!ret.get().buffer || !ret.get().mapped) { throw Error{"Failed to write create Vulkan host buffer"}; }
	std::memcpy(ret.get().mapped, bytes.data(), bytes.size());
	return BufferView{.buffer = ret.get().buffer, .size = bytes.size(), .count = count};
}
} // namespace levk::vulkan
