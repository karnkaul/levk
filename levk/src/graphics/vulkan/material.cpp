#include <graphics/vulkan/material.hpp>
#include <graphics/vulkan/pipeline.hpp>
#include <levk/asset/shader_provider.hpp>

namespace levk::vulkan {
bool Material::build_layout(PipelineBuilder& pipeline_builder, Uri<ShaderCode> const& vert, Uri<ShaderCode> const& frag) {
	auto* pipeline_layout = pipeline_builder.try_build_layout(vert, frag);
	if (!pipeline_layout) { return false; }
	shader_layout = pipeline_layout->shader_layout();
	return true;
}
} // namespace levk::vulkan
