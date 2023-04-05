#pragma once
#include <graphics/vulkan/common.hpp>
#include <levk/uri.hpp>

namespace levk {
struct ShaderCode;
class ShaderProvider;

namespace vulkan {
struct SpirV {
	std::span<std::uint32_t const> code{};
	std::size_t hash{};

	static SpirV from(ShaderProvider& provider, Uri<ShaderCode> const& uri);

	explicit operator bool() const { return !code.empty() && hash > 0; }
};

struct PipelineState {
	vk::PolygonMode mode{vk::PolygonMode::eFill};
	vk::PrimitiveTopology topology{vk::PrimitiveTopology::eTriangleList};
	bool depth_test{true};

	bool operator==(PipelineState const&) const = default;
};

struct Pipeline {
	vk::Pipeline pipeline{};
	vk::PipelineLayout layout{};
	std::span<SetLayout const> set_layouts{};
	std::span<vk::DescriptorSetLayout const> descriptor_set_layouts{};

	explicit operator bool() const { return pipeline && layout; }

	void bind(vk::CommandBuffer cb, vk::Extent2D extent, float line_width = 1.0f);
};

struct PipelineInfo {
	vk::Format colour_format;
	SpirV vert;
	SpirV frag;

	VertexInput::View vertex_input{};
	PipelineState state{};
};

struct PipelineCreateInfo {
	VertexInput::View vinput;
	vk::ShaderModule vert;
	vk::ShaderModule frag;
	vk::PipelineLayout layout;
	PipelineFormat format;

	PipelineState state{};
	bool sample_shading{true};
};

struct PipelineLayout {
	vk::UniquePipelineLayout pipeline_layout{};
	std::vector<SetLayout> set_layouts{};
	std::vector<vk::UniqueDescriptorSetLayout> set_layouts_storage{};
	std::vector<vk::DescriptorSetLayout> descriptor_set_layouts{};
	vk::UniqueShaderModule vert{};
	vk::UniqueShaderModule frag{};
	ShaderHash hash{};

	static ShaderHash make_hash(VertFrag<SpirV> vf);
	static PipelineLayout make(vk::Device device, VertFrag<SpirV> vf);

	ShaderLayout shader_layout() const { return {set_layouts, hash}; }
};

struct PipelineFixedState {
	PipelineState state{};
	std::size_t vertex_input_hash{};
	PipelineFormat format{};

	bool operator==(PipelineFixedState const&) const = default;

	struct Hasher {
		std::size_t operator()(PipelineFixedState const& state) const;
	};
};

struct PipelineMap {
	PipelineLayout layout{};
	std::unordered_map<PipelineFixedState, vk::UniquePipeline, PipelineFixedState::Hasher> pipelines{};
};

struct PipelineStorage {
	std::unordered_map<ShaderHash, PipelineMap, ShaderHash::Hasher> maps{};
	bool sample_rate_shading{};

	std::mutex mutex{};
};

struct PipelineBuilder {
	PipelineCreateInfo create_info{};
	PipelineStorage& out;
	ShaderProvider& shader_provider;
	vk::Device device;

	PipelineBuilder(PipelineStorage& out, ShaderProvider& shader_provider, vk::Device device, PipelineFormat format);

	Pipeline try_build(VertexInput::View vertex_input, PipelineState state, ShaderHash shader_hash);
	Ptr<PipelineLayout> try_build_layout(Uri<ShaderCode> const& vert, Uri<ShaderCode> const& frag);
};
} // namespace vulkan
} // namespace levk
