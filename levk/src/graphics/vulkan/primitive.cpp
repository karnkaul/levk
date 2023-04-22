#include <graphics/vulkan/ad_hoc_cmd.hpp>
#include <graphics/vulkan/primitive.hpp>
#include <graphics/vulkan/render_object.hpp>
#include <levk/util/error.hpp>

namespace levk::vulkan {
namespace {
struct GeometryUploader {
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
		if (!out_buffer.get().buffer || !out_buffer.get().mapped) { throw Error{"Failed to write create Vulkan staging buffer"}; }
		auto writer = BufferWriter{out_buffer};
		out_layout.offsets.positions = writer(std::span{geometry.positions});
		out_layout.offsets.rgbs = writer(std::span{geometry.rgbs});
		out_layout.offsets.normals = writer(std::span{geometry.normals});
		out_layout.offsets.uvs = writer(std::span{geometry.uvs});
		if (!indices.empty()) { out_layout.offsets.indices = writer(indices); }
		out_layout.vertex_input = VertexInput::for_static();
		out_layout.instances_binding = RenderObject::Instances::vertex_binding_v;
	}

	void write_to(GeometryLayout& out_layout, UniqueBuffer& out_buffer, MeshJoints const& joints) const {
		assert(joints.joints.size() >= joints.weights.size());
		auto const size = joints.joints.size_bytes() + joints.weights.size_bytes();
		if (out_buffer.get().size < size) { out_buffer = vma.make_buffer(v_flags_v, size, false); }
		if (!out_buffer.get().buffer || !out_buffer.get().mapped) { throw Error{"Failed to write create Vulkan staging buffer"}; }
		auto writer = BufferWriter{out_buffer};
		out_layout.offsets.joints = writer(joints.joints);
		out_layout.offsets.weights = writer(joints.weights);
		out_layout.vertex_input = VertexInput::for_skinned();
		out_layout.joints_binding = RenderObject::Joints::vertex_binding_v;
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
	m_layout.vertex_input = VertexInput::for_static();
	m_layout.instances_binding = RenderObject::Instances::vertex_binding_v;
}

Vma::Buffer const& HostPrimitive::refresh() {
	auto& v = m_vibo.buffers.get()[*m_device.buffered_index];
	GeometryUploader{m_device.vma}.write_to(m_layout, v, geometry);
	return v.get();
}
} // namespace levk::vulkan
