#pragma once
#include <graphics/vulkan/common.hpp>

namespace levk::vulkan {
struct ImageBarrier {
	vk::ImageMemoryBarrier2 barrier{};

	ImageBarrier(vk::Image image, std::uint32_t mip_levels = 1u, std::uint32_t array_layers = 1u) {
		barrier.image = image;
		barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, mip_levels, 0, array_layers};
	}

	ImageBarrier(Vma::Image const& image) : ImageBarrier(image.image, image.mip_levels, image.array_layers) {}

	ImageBarrier& set_full_barrier(vk::ImageLayout src, vk::ImageLayout dst);
	ImageBarrier& set_undef_to_optimal(bool depth);
	ImageBarrier& set_undef_to_transfer_dst();
	ImageBarrier& set_optimal_to_read_only(bool depth);
	ImageBarrier& set_optimal_to_transfer_src();
	ImageBarrier& set_optimal_to_present();
	ImageBarrier& set_transfer_dst_to_optimal(bool depth);
	ImageBarrier& set_transfer_dst_to_present();

	void transition(vk::CommandBuffer cb) const {
		if (!barrier.image) { return; }
		transition(cb, {&barrier, 1u});
	}

	static void transition(vk::CommandBuffer cb, std::span<vk::ImageMemoryBarrier2 const> barriers) {
		if (barriers.empty()) { return; }
		auto di = vk::DependencyInfo{};
		di.imageMemoryBarrierCount = static_cast<std::uint32_t>(barriers.size());
		di.pImageMemoryBarriers = barriers.data();
		cb.pipelineBarrier2(di);
	}
};
} // namespace levk::vulkan
