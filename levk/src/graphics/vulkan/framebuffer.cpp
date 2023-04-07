#include <graphics/vulkan/framebuffer.hpp>
#include <graphics/vulkan/image_barrier.hpp>
#include <levk/util/flex_array.hpp>

namespace levk::vulkan {
PipelineFormat Framebuffer::pipeline_format() const {
	return PipelineFormat{
		.colour = colour.format,
		.depth = depth.format,
		.samples = samples,
	};
}

void Framebuffer::undef_to_optimal(vk::CommandBuffer cb) const {
	auto barriers = FlexArray<vk::ImageMemoryBarrier2, 4>{};
	if (colour.image) { barriers.insert(ImageBarrier{colour.image}.set_undef_to_optimal(false).barrier); }
	if (depth.image) { barriers.insert(ImageBarrier{depth.image}.set_undef_to_optimal(true).barrier); }
	if (resolve.image) { barriers.insert(ImageBarrier{resolve.image}.set_undef_to_optimal(false).barrier); }
	ImageBarrier::transition(cb, barriers.span());
}

void Framebuffer::undef_to_transfer_dst(vk::CommandBuffer cb) const { ImageBarrier{output().image}.set_undef_to_transfer_dst().transition(cb); }

void Framebuffer::optimal_to_read_only(vk::CommandBuffer cb) const {
	auto barriers = FlexArray<vk::ImageMemoryBarrier2, 2>{};
	barriers.insert(ImageBarrier{output().image}.set_optimal_to_read_only(false).barrier);
	if (depth.image) { barriers.insert(ImageBarrier{depth.image}.set_undef_to_optimal(true).barrier); }
	ImageBarrier::transition(cb, barriers.span());
}

void Framebuffer::optimal_to_transfer_src(vk::CommandBuffer cb) const { ImageBarrier{output().image}.set_optimal_to_transfer_src().transition(cb); }

void Framebuffer::optimal_to_present(vk::CommandBuffer cb) const { ImageBarrier{output().image}.set_optimal_to_present().transition(cb); }

void Framebuffer::transfer_dst_to_optimal(vk::CommandBuffer cb) const {
	auto barriers = FlexArray<vk::ImageMemoryBarrier2, 4>{};
	if (colour.image) { barriers.insert(ImageBarrier{colour.image}.set_transfer_dst_to_optimal(false).barrier); }
	if (depth.image) { barriers.insert(ImageBarrier{depth.image}.set_transfer_dst_to_optimal(true).barrier); }
	if (resolve.image) { barriers.insert(ImageBarrier{resolve.image}.set_transfer_dst_to_optimal(false).barrier); }
	ImageBarrier::transition(cb, barriers.span());
}

void Framebuffer::transfer_dst_to_present(vk::CommandBuffer cb) const { ImageBarrier{output().image}.set_transfer_dst_to_present().transition(cb); }

void Framebuffer::set_output(ImageView acquired) {
	if (samples > vk::SampleCountFlagBits::e1) {
		resolve = acquired;
	} else {
		colour = acquired;
	}
}

void Framebuffer::begin_render(std::optional<glm::vec4> const& clear, vk::CommandBuffer cb) {
	auto ri = vk::RenderingInfo{};
	ri.renderArea = vk::Rect2D{{}, output().extent};
	ri.layerCount = 1u;

	auto colour_attachment = vk::RenderingAttachmentInfo{colour.view, vk::ImageLayout::eAttachmentOptimal};
	colour_attachment.loadOp = vk::AttachmentLoadOp::eLoad;
	colour_attachment.storeOp = vk::AttachmentStoreOp::eStore;
	if (clear) {
		colour_attachment.loadOp = vk::AttachmentLoadOp::eClear;
		colour_attachment.clearValue = vk::ClearColorValue{std::array{clear->x, clear->y, clear->z, clear->w}};
	}
	if (resolve.view) {
		colour_attachment.resolveImageView = resolve.view;
		colour_attachment.resolveImageLayout = vk::ImageLayout::eAttachmentOptimal;
		colour_attachment.resolveMode = vk::ResolveModeFlagBits::eAverage;
	}
	ri.colorAttachmentCount = 1u;
	ri.pColorAttachments = &colour_attachment;

	auto depth_attachment = vk::RenderingAttachmentInfo{depth.view, vk::ImageLayout::eAttachmentOptimal};
	if (depth.view) {
		depth_attachment.loadOp = vk::AttachmentLoadOp::eClear;
		depth_attachment.storeOp = vk::AttachmentStoreOp::eDontCare;
		depth_attachment.clearValue = vk::ClearDepthStencilValue{1.0f, 0};
		ri.pDepthAttachment = &depth_attachment;
	}

	cb.beginRendering(ri);
}

void Framebuffer::end_render(vk::CommandBuffer cb) { cb.endRendering(); }

PipelineFormat Depthbuffer::pipeline_format() const {
	return PipelineFormat{
		.depth = image.format,
		.samples = samples,
	};
}

void Depthbuffer::undef_to_optimal(vk::CommandBuffer cb) const { ImageBarrier{image.image}.set_undef_to_optimal(true).transition(cb); }
void Depthbuffer::optimal_to_read_only(vk::CommandBuffer cb) const { ImageBarrier{image.image}.set_optimal_to_read_only(true).transition(cb); }

void Depthbuffer::begin_render(vk::CommandBuffer cb) {
	auto ri = vk::RenderingInfo{};
	ri.renderArea = vk::Rect2D{{}, image.extent};
	ri.layerCount = 1u;

	auto depth_attachment = vk::RenderingAttachmentInfo{image.view, vk::ImageLayout::eAttachmentOptimal};
	depth_attachment.loadOp = vk::AttachmentLoadOp::eClear;
	depth_attachment.storeOp = vk::AttachmentStoreOp::eStore;
	depth_attachment.clearValue = vk::ClearDepthStencilValue{1.0f, 0};
	ri.pDepthAttachment = &depth_attachment;

	cb.beginRendering(ri);
}

void Depthbuffer::end_render(vk::CommandBuffer cb) { cb.endRendering(); }
} // namespace levk::vulkan
