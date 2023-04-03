#include <graphics/vulkan/image_barrier.hpp>

namespace levk::vulkan {
ImageBarrier& ImageBarrier::set_full_barrier(vk::ImageLayout src, vk::ImageLayout dst) {
	barrier.oldLayout = src;
	barrier.newLayout = dst;
	barrier.srcStageMask = barrier.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands;
	barrier.srcAccessMask = barrier.dstAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite;
	return *this;
}

ImageBarrier& ImageBarrier::set_undef_to_optimal(bool depth) {
	barrier.oldLayout = vk::ImageLayout::eUndefined;
	barrier.newLayout = vk::ImageLayout::eAttachmentOptimal;
	barrier.srcStageMask = vk::PipelineStageFlagBits2::eNone;
	barrier.srcAccessMask = vk::AccessFlagBits2::eNone;
	if (depth) {
		barrier.dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
		barrier.dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead;
		barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
	} else {
		barrier.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
		barrier.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead;
	}
	return *this;
}

ImageBarrier& ImageBarrier::set_undef_to_transfer_dst() {
	barrier.oldLayout = vk::ImageLayout::eUndefined;
	barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.srcStageMask = vk::PipelineStageFlagBits2::eNone;
	barrier.srcAccessMask = vk::AccessFlagBits2::eNone;
	barrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
	barrier.dstAccessMask = vk::AccessFlagBits2::eTransferWrite | vk::AccessFlagBits2::eTransferRead;
	return *this;
}

ImageBarrier& ImageBarrier::set_optimal_to_read_only(bool depth) {
	barrier.oldLayout = vk::ImageLayout::eAttachmentOptimal;
	barrier.newLayout = vk::ImageLayout::eReadOnlyOptimal;
	if (depth) {
		barrier.srcStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
		barrier.srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead;
		barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
	} else {
		barrier.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
		barrier.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead;
	}
	barrier.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
	barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
	return *this;
}

ImageBarrier& ImageBarrier::set_optimal_to_transfer_src() {
	barrier.oldLayout = vk::ImageLayout::eAttachmentOptimal;
	barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
	barrier.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
	barrier.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead;
	barrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
	barrier.dstAccessMask = vk::AccessFlagBits2::eTransferRead | vk::AccessFlagBits2::eTransferWrite;
	return *this;
}

ImageBarrier& ImageBarrier::set_optimal_to_present() {
	barrier.oldLayout = vk::ImageLayout::eAttachmentOptimal;
	barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
	barrier.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
	barrier.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead;
	barrier.dstStageMask = vk::PipelineStageFlagBits2::eNone;
	barrier.dstAccessMask = vk::AccessFlagBits2::eNone;
	return *this;
}

ImageBarrier& ImageBarrier::set_transfer_dst_to_optimal(bool depth) {
	barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.newLayout = vk::ImageLayout::eAttachmentOptimal;
	barrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
	barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite | vk::AccessFlagBits2::eTransferRead;
	if (depth) {
		barrier.dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests;
		barrier.dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead;
		barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
	} else {
		barrier.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
		barrier.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite;
	}
	return *this;
}

ImageBarrier& ImageBarrier::set_transfer_dst_to_present() {
	barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
	barrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
	barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite | vk::AccessFlagBits2::eTransferRead;
	barrier.dstStageMask = vk::PipelineStageFlagBits2::eNone;
	barrier.dstAccessMask = vk::AccessFlagBits2::eNone;
	return *this;
}
} // namespace levk::vulkan
