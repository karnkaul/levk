#pragma once
#include <graphics/vulkan/common.hpp>

namespace levk::vulkan {
struct PipelineBuilder;

struct Material {
	ShaderLayout shader_layout{};

	bool build_layout(PipelineBuilder& pipeline_builder, Uri<ShaderCode> const& vert, Uri<ShaderCode> const& frag);
};
} // namespace levk::vulkan
