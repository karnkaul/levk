#include <glm/vec2.hpp>
#include <graphics/vulkan/pipeline.hpp>
#include <levk/asset/shader_provider.hpp>
#include <levk/util/error.hpp>
#include <levk/util/hash_combine.hpp>
#include <spirv_glsl.hpp>
#include <map>

namespace levk::vulkan {
namespace {
std::vector<SetLayout> make_set_layouts(VertFrag<std::span<std::uint32_t const>> vf) {
	static constexpr auto stage_flags_v = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
	std::map<std::uint32_t, std::map<std::uint32_t, vk::DescriptorSetLayoutBinding>> set_layout_bindings{};
	auto populate = [&set_layout_bindings](std::span<std::uint32_t const> code) {
		auto compiler = spirv_cross::CompilerGLSL{code.data(), code.size()};
		auto resources = compiler.get_shader_resources();
		auto set_resources = [&compiler, &set_layout_bindings](std::span<spirv_cross::Resource const> resources, vk::DescriptorType const type) {
			for (auto const& resource : resources) {
				auto const set_number = compiler.get_decoration(resource.id, spv::Decoration::DecorationDescriptorSet);
				auto& layout = set_layout_bindings[set_number];
				auto const binding_number = compiler.get_decoration(resource.id, spv::Decoration::DecorationBinding);
				auto& dslb = layout[binding_number];
				dslb.binding = binding_number;
				dslb.descriptorType = type;
				dslb.stageFlags = stage_flags_v;
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
	populate(vf.vert);
	populate(vf.frag);

	for (auto& [_, bindings] : set_layout_bindings) {
		for (auto& [_, binding] : bindings) { binding.stageFlags |= stage_flags_v; }
	}

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
	return ret;
}

vk::UniqueShaderModule make_shader_module(vk::Device device, SpirV const& spirv) {
	assert(spirv.hash > 0 && !spirv.code.empty());
	auto smci = vk::ShaderModuleCreateInfo{{}, spirv.code.size_bytes(), spirv.code.data()};
	return device.createShaderModuleUnique(smci);
}

vk::UniquePipelineLayout make_pipeline_layout(vk::Device device, std::span<vk::DescriptorSetLayout const> set_layouts) {
	auto plci = vk::PipelineLayoutCreateInfo{};
	plci.setLayoutCount = static_cast<std::uint32_t>(set_layouts.size());
	plci.pSetLayouts = set_layouts.data();
	return device.createPipelineLayoutUnique(plci);
}

vk::UniquePipeline make_pipeline(vk::Device device, PipelineCreateInfo const& info) {
	auto gpci = vk::GraphicsPipelineCreateInfo{};
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
	pmssci.rasterizationSamples = info.format.samples;
	pmssci.sampleShadingEnable = info.sample_shading;
	gpci.pMultisampleState = &pmssci;

	auto prci = vk::PipelineRenderingCreateInfo{};
	if (info.format.colour != vk::Format{}) {
		prci.colorAttachmentCount = 1u;
		prci.pColorAttachmentFormats = &info.format.colour;
	}
	prci.depthAttachmentFormat = info.format.depth;
	gpci.pNext = &prci;

	auto ret = vk::Pipeline{};
	if (device.createGraphicsPipelines({}, 1u, &gpci, {}, &ret) != vk::Result::eSuccess) { throw Error{"Failed to create graphics pipeline"}; }

	return vk::UniquePipeline{ret, device};
}
} // namespace

SpirV SpirV::from(ShaderProvider& provider, Uri<ShaderCode> const& uri) {
	auto data = provider.load(uri);
	if (!data) { return {}; }
	return {data->spir_v.span(), data->hash};
}

void Pipeline::bind(vk::CommandBuffer cb, vk::Extent2D extent, float line_width, bool negative_viewport) {
	cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
	glm::vec2 const fextent = glm::uvec2{extent.width, extent.height};
	auto viewport = vk::Viewport{0.0f, fextent.y, fextent.x, -fextent.y, 0.0f, 1.0f};
	if (!negative_viewport) {
		viewport.y = {};
		viewport.height = fextent.y;
	}
	auto const scissor = vk::Rect2D{{}, extent};
	cb.setViewport(0u, viewport);
	cb.setScissor(0u, scissor);
	cb.setLineWidth(line_width);
}

ShaderHash PipelineLayout::make_hash(VertFrag<SpirV> vf) { return {.value = make_combined_hash(vf.vert.hash, vf.frag.hash)}; }

PipelineLayout PipelineLayout::make(vk::Device device, VertFrag<SpirV> vf) {
	auto ret = PipelineLayout{};
	ret.set_layouts = make_set_layouts({vf.vert.code, vf.frag.code});
	ret.set_layouts_storage.reserve(ret.set_layouts.size());
	ret.descriptor_set_layouts.reserve(ret.set_layouts.size());
	for (auto const& layout : ret.set_layouts) {
		auto dslci = vk::DescriptorSetLayoutCreateInfo{};
		dslci.bindingCount = static_cast<std::uint32_t>(layout.bindings.span().size());
		dslci.pBindings = layout.bindings.span().data();
		ret.set_layouts_storage.push_back(device.createDescriptorSetLayoutUnique(dslci));
		ret.descriptor_set_layouts.push_back(*ret.set_layouts_storage.back());
	}

	ret.pipeline_layout = make_pipeline_layout(device, ret.descriptor_set_layouts);
	ret.vert = make_shader_module(device, vf.vert);
	ret.frag = make_shader_module(device, vf.frag);
	ret.hash = make_hash(vf);
	return ret;
}

std::size_t PipelineFormat::Hasher::operator()(PipelineFormat const& f) const { return make_combined_hash(f.colour, f.depth, f.samples); }

std::size_t PipelineFixedState::Hasher::operator()(PipelineFixedState const& state) const {
	auto const ret = PipelineFormat::Hasher{}(state.format);
	return make_combined_hash(state.state.depth_test, state.state.mode, state.state.topology, state.vertex_input_hash, ret);
}

PipelineBuilder::PipelineBuilder(PipelineStorage& out, ShaderProvider& shader_provider, vk::Device device, PipelineFormat format)
	: out(out), shader_provider(shader_provider), device(device) {
	create_info.format = format;
}

Pipeline PipelineBuilder::try_build(VertexInput::View vertex_input, PipelineState state, ShaderHash shader_hash) {
	auto const it = out.maps.find(shader_hash);
	if (it == out.maps.end()) { return {}; }
	auto& map = it->second;
	auto const fixed_state = PipelineFixedState{create_info.state, create_info.vinput.hash, create_info.format};
	auto jt = map.pipelines.find(fixed_state);
	if (jt == map.pipelines.end()) {
		create_info.vinput = vertex_input;
		create_info.state = state;
		create_info.layout = *map.layout.pipeline_layout;
		create_info.vert = *map.layout.vert;
		create_info.frag = *map.layout.frag;
		auto ret = make_pipeline(device, create_info);
		if (!ret) { return {}; }
		auto [j, _] = map.pipelines.insert_or_assign(fixed_state, std::move(ret));
		jt = j;
	}
	assert(jt != map.pipelines.end());
	return {
		.pipeline = *jt->second,
		.layout = *map.layout.pipeline_layout,
		.set_layouts = map.layout.set_layouts,
		.descriptor_set_layouts = map.layout.descriptor_set_layouts,
	};
}

Ptr<PipelineLayout> PipelineBuilder::try_build_layout(Uri<ShaderCode> const& vert, Uri<ShaderCode> const& frag) {
	auto const shader = VertFrag<SpirV>{
		.vert = SpirV::from(shader_provider, vert),
		.frag = SpirV::from(shader_provider, frag),
	};
	if (!shader.vert || !shader.frag) { return {}; }
	auto const shader_hash = PipelineLayout::make_hash(shader);
	auto& map = out.maps[shader_hash];
	if (!map.layout.pipeline_layout) { map.layout = PipelineLayout::make(device, shader); }
	return &map.layout;
}
} // namespace levk::vulkan
