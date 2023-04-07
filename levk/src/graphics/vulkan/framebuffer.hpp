#pragma once
#include <graphics/vulkan/common.hpp>

namespace levk::vulkan {
struct Framebuffer {
	ImageView colour{};
	ImageView depth{};
	ImageView resolve{};
	vk::SampleCountFlagBits samples{vk::SampleCountFlagBits::e1};

	ImageView const& output() const { return resolve.image ? resolve : colour; }

	PipelineFormat pipeline_format() const;

	void undef_to_optimal(vk::CommandBuffer cb) const;
	void undef_to_transfer_dst(vk::CommandBuffer cb) const;
	void optimal_to_read_only(vk::CommandBuffer cb) const;
	void optimal_to_transfer_src(vk::CommandBuffer cb) const;
	void optimal_to_present(vk::CommandBuffer cb) const;
	void transfer_dst_to_optimal(vk::CommandBuffer cb) const;
	void transfer_dst_to_present(vk::CommandBuffer cb) const;

	void set_output(ImageView acquired);
	void begin_render(std::optional<glm::vec4> const& clear, vk::CommandBuffer cb);
	void end_render(vk::CommandBuffer cb);
};

struct Depthbuffer {
	ImageView image{};
	vk::SampleCountFlagBits samples{vk::SampleCountFlagBits::e1};

	PipelineFormat pipeline_format() const;

	void undef_to_optimal(vk::CommandBuffer cb) const;
	void optimal_to_read_only(vk::CommandBuffer cb) const;

	void begin_render(vk::CommandBuffer cb);
	void end_render(vk::CommandBuffer cb);
};
} // namespace levk::vulkan
