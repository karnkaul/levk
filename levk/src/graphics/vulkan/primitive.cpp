#include <graphics/vulkan/ad_hoc_cmd.hpp>
#include <graphics/vulkan/primitive.hpp>

namespace levk::vulkan {
namespace {
VertexInput common_vertex_input() {
	auto ret = VertexInput{};
	// position
	ret.bindings.insert(vk::VertexInputBindingDescription{0, sizeof(glm::vec3)});
	ret.attributes.insert(vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32B32Sfloat});
	// rgb
	ret.bindings.insert(vk::VertexInputBindingDescription{1, sizeof(glm::vec3)});
	ret.attributes.insert(vk::VertexInputAttributeDescription{1, 1, vk::Format::eR32G32B32Sfloat});
	// normal
	ret.bindings.insert(vk::VertexInputBindingDescription{2, sizeof(glm::vec3)});
	ret.attributes.insert(vk::VertexInputAttributeDescription{2, 2, vk::Format::eR32G32B32Sfloat});
	// uv
	ret.bindings.insert(vk::VertexInputBindingDescription{3, sizeof(glm::vec2)});
	ret.attributes.insert(vk::VertexInputAttributeDescription{3, 3, vk::Format::eR32G32Sfloat});
	return ret;
}

VertexInput instanced_vertex_input() {
	auto ret = common_vertex_input();

	// instance matrix
	ret.bindings.insert(vk::VertexInputBindingDescription{4, sizeof(glm::mat4), vk::VertexInputRate::eInstance});
	ret.attributes.insert(vk::VertexInputAttributeDescription{4, 4, vk::Format::eR32G32B32A32Sfloat, 0 * sizeof(glm::vec4)});
	ret.attributes.insert(vk::VertexInputAttributeDescription{5, 4, vk::Format::eR32G32B32A32Sfloat, 1 * sizeof(glm::vec4)});
	ret.attributes.insert(vk::VertexInputAttributeDescription{6, 4, vk::Format::eR32G32B32A32Sfloat, 2 * sizeof(glm::vec4)});
	ret.attributes.insert(vk::VertexInputAttributeDescription{7, 4, vk::Format::eR32G32B32A32Sfloat, 3 * sizeof(glm::vec4)});

	return ret;
}

VertexInput skinned_vertex_input() {
	auto ret = instanced_vertex_input();

	// joints
	ret.bindings.insert(vk::VertexInputBindingDescription{8, sizeof(glm::uvec4)});
	ret.attributes.insert(vk::VertexInputAttributeDescription{8, 8, vk::Format::eR32G32B32A32Uint});
	// weights
	ret.bindings.insert(vk::VertexInputBindingDescription{9, sizeof(glm::vec4)});
	ret.attributes.insert(vk::VertexInputAttributeDescription{9, 9, vk::Format::eR32G32B32A32Sfloat});

	return ret;
}

struct GeometryUploader {
	struct Bindings {
		static constexpr auto instance_v{4u};
		static constexpr auto joints_v{8u};
	};

	struct Sets {
		static constexpr auto joints_v{3u};
	};

	static constexpr auto v_flags_v = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
	static constexpr auto vi_flags_v = v_flags_v | vk::BufferUsageFlagBits::eIndexBuffer;

	Vma const& vma;

	FlexArray<UniqueBuffer, 2> staging_buffers{};

	void write_to(GeometryLayout& out_layout, UniqueBuffer& out_buffer, Geometry::Packed const& geometry) const {
		auto const indices = std::span<std::uint32_t const>{geometry.indices};
		out_layout.vertices = static_cast<std::uint32_t>(geometry.positions.size());
		out_layout.indices = static_cast<std::uint32_t>(indices.size());
		auto const size = geometry.size_bytes();
		if (out_buffer.get().size < size) { out_buffer = vma.make_buffer(vi_flags_v, size, true); }
		vma.with_mapped(out_buffer.get(), [&](void*) {
			auto writer = BufferWriter{out_buffer};
			out_layout.offsets.positions = writer(std::span{geometry.positions});
			out_layout.offsets.rgbs = writer(std::span{geometry.rgbs});
			out_layout.offsets.normals = writer(std::span{geometry.normals});
			out_layout.offsets.uvs = writer(std::span{geometry.uvs});
			if (!indices.empty()) { out_layout.offsets.indices = writer(indices); }
		});
		out_layout.vertex_input = instanced_vertex_input();
		out_layout.instance_binding = Bindings::instance_v;
	}

	void write_to(GeometryLayout& out_layout, UniqueBuffer& out_buffer, MeshJoints const& joints) const {
		assert(joints.joints.size() >= joints.weights.size());
		auto const size = joints.joints.size_bytes() + joints.weights.size_bytes();
		if (out_buffer.get().size < size) { out_buffer = vma.make_buffer(v_flags_v, size, false); }
		vma.with_mapped(out_buffer.get(), [&](void*) {
			auto writer = BufferWriter{out_buffer};
			out_layout.offsets.joints = writer(joints.joints);
			out_layout.offsets.weights = writer(joints.weights);
		});
		out_layout.vertex_input = skinned_vertex_input();
		out_layout.joints_binding = Bindings::joints_v;
		out_layout.joints_set = Sets::joints_v;
		out_layout.joints = static_cast<std::uint32_t>(joints.joints.size());
	}

	[[nodiscard]] UniqueBuffer upload(GeometryLayout& out_result, vk::CommandBuffer cb, Geometry::Packed const& geometry) {
		auto const size = geometry.size_bytes();
		auto staging = vma.make_buffer(vk::BufferUsageFlagBits::eTransferSrc, size, true);
		auto ret = vma.make_buffer(vk::BufferUsageFlagBits::eTransferDst | vi_flags_v, size, false);
		write_to(out_result, staging, geometry);
		cb.copyBuffer(staging.get().buffer, ret.get().buffer, vk::BufferCopy{{}, {}, size});
		staging_buffers.insert(std::move(staging));
		return ret;
	}

	[[nodiscard]] UniqueBuffer upload(GeometryLayout& out_result, vk::CommandBuffer cb, MeshJoints const& joints) {
		auto const size = joints.joints.size_bytes() + joints.weights.size_bytes();
		auto staging = vma.make_buffer(vk::BufferUsageFlagBits::eTransferSrc, size, true);
		auto ret = vma.make_buffer(vk::BufferUsageFlagBits::eTransferDst | v_flags_v, size, false);
		write_to(out_result, staging, joints);
		cb.copyBuffer(staging.get().buffer, ret.get().buffer, vk::BufferCopy{{}, {}, size});
		staging_buffers.insert(std::move(staging));
		return ret;
	}
};
} // namespace

auto UploadedPrimitive::make_static(DeviceView const& device, Geometry::Packed const& geometry) -> UploadedPrimitive {
	auto ret = UploadedPrimitive{};
	ret.m_vibo = DeviceBuffer::make(device, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer);
	auto uploader = GeometryUploader{device.vma};
	auto cmd = AdHocCmd{device};
	ret.m_vibo.buffer.get() = uploader.upload(ret.m_layout, cmd.cb, geometry);
	return ret;
}

auto UploadedPrimitive::make_skinned(DeviceView const& device, Geometry::Packed const& geometry, MeshJoints const& joints) -> UploadedPrimitive {
	assert(!joints.joints.empty());
	auto ret = UploadedPrimitive{};
	ret.m_vibo = DeviceBuffer::make(device, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer);
	ret.m_jwbo = DeviceBuffer::make(device, vk::BufferUsageFlagBits::eVertexBuffer);
	auto uploader = GeometryUploader{device.vma};
	auto cmd = AdHocCmd{device};
	ret.m_vibo.buffer.get() = uploader.upload(ret.m_layout, cmd.cb, geometry);
	ret.m_jwbo.buffer.get() = uploader.upload(ret.m_layout, cmd.cb, joints);
	return ret;
}

HostPrimitive::HostPrimitive(DeviceView const& device)
	: m_vibo(HostBuffer::make(device, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer)), m_device(device) {
	m_layout.vertex_input = instanced_vertex_input();
	m_layout.instance_binding = GeometryUploader::Bindings::instance_v;
}

Vma::Buffer const& HostPrimitive::refresh() {
	auto& v = m_vibo.buffers.get()[*m_device.buffered_index];
	GeometryUploader{m_device.vma}.write_to(m_layout, v, geometry);
	return v.get();
}
} // namespace levk::vulkan
