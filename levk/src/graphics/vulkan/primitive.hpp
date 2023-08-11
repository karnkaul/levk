#pragma once
#include <glm/mat4x4.hpp>
#include <graphics/vulkan/common.hpp>
#include <levk/graphics/primitive.hpp>

namespace levk::vulkan {
class UploadedPrimitive;
class HostPrimitive;

struct Primitive {
	virtual ~Primitive() = default;

	Primitive() = default;
	Primitive(Primitive&&) = default;
	Primitive& operator=(Primitive&&) = default;

	GeometryLayout const& layout() const { return m_layout; }

	void draw(Vma::Buffer const& vibo, vk::CommandBuffer cb, std::uint32_t instances = 1u) const {
		if (!vibo.buffer) { return; }
		vk::Buffer const vbos[] = {vibo.buffer, vibo.buffer, vibo.buffer, vibo.buffer};
		vk::DeviceSize const vbo_offsets[] = {m_layout.offsets.positions, m_layout.offsets.rgbas, m_layout.offsets.normals, m_layout.offsets.uvs};
		cb.bindVertexBuffers(0u, vbos, vbo_offsets);
		if (m_layout.indices > 0) {
			cb.bindIndexBuffer(vibo.buffer, m_layout.offsets.indices, vk::IndexType::eUint32);
			cb.drawIndexed(m_layout.indices, instances, 0u, 0u, 0u);
		} else {
			cb.draw(m_layout.vertices, instances, 0u, 0u);
		}
	}

	void draw(Vma::Buffer const& vibo, Vma::Buffer const& jwbo, vk::CommandBuffer cb, std::uint32_t instances = 1u) const {
		if (!vibo.buffer) { return; }
		assert(m_layout.joints_binding > 0);
		vk::Buffer const jbos[] = {jwbo.buffer, jwbo.buffer};
		vk::DeviceSize const jbo_offsets[] = {m_layout.offsets.joints, m_layout.offsets.weights};
		cb.bindVertexBuffers(*m_layout.joints_binding, jbos, jbo_offsets);
		draw(vibo, cb, instances);
	}

	virtual void draw(vk::CommandBuffer cb, std::uint32_t instances = 1u) = 0;

  protected:
	GeometryLayout m_layout{};
};

class UploadedPrimitive : public Primitive {
  public:
	static UploadedPrimitive make_static(DeviceView const& device, Geometry::Packed const& geometry);
	static UploadedPrimitive make_skinned(DeviceView const& device, Geometry::Packed const& geometry, MeshJoints const& joints);

  private:
	UploadedPrimitive() = default;

	void draw(vk::CommandBuffer cb, std::uint32_t instances = 1u) final {
		if (m_layout.joints_binding) {
			assert(m_jwbo.buffer.get());
			Primitive::draw(m_vibo.buffer.get().get(), m_jwbo.buffer.get().get(), cb, instances);
		} else {
			Primitive::draw(m_vibo.buffer.get().get(), cb, instances);
		}
	}

	DeviceBuffer m_vibo{};
	DeviceBuffer m_jwbo{};
};

class HostPrimitive : public Primitive {
  public:
	HostPrimitive(DeviceView const& device);

	Geometry::Packed geometry{};

  private:
	void draw(vk::CommandBuffer cb, std::uint32_t instances = 1u) final {
		if (geometry.positions.empty()) { return; }
		Primitive::draw(refresh(), cb, instances);
	}

	Vma::Buffer const& refresh();

	HostBuffer m_vibo{};
	DeviceView m_device{};
};
} // namespace levk::vulkan
