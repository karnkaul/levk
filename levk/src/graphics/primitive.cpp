#include <graphics/vulkan/device.hpp>
#include <graphics/vulkan/primitive.hpp>
#include <levk/graphics/primitive.hpp>

namespace levk {
void StaticPrimitive::Deleter::operator()(vulkan::UploadedPrimitive const* ptr) const { delete ptr; }

StaticPrimitive::StaticPrimitive(vulkan::Device& device, Geometry::Packed const& geometry)
	: m_primitive(new vulkan::UploadedPrimitive{vulkan::UploadedPrimitive::make_static(device.view(), geometry)}) {}

std::uint32_t StaticPrimitive::vertex_count() const { return m_primitive->layout().vertices; }
std::uint32_t StaticPrimitive::index_count() const { return m_primitive->layout().indices; }

Ptr<vulkan::Primitive> StaticPrimitive::vulkan_primitive() const { return m_primitive.get(); }

void SkinnedPrimitive::Deleter::operator()(vulkan::UploadedPrimitive const* ptr) const { delete ptr; }

SkinnedPrimitive::SkinnedPrimitive(vulkan::Device& device, Geometry::Packed const& geometry, MeshJoints const& joints)
	: m_primitive(new vulkan::UploadedPrimitive{vulkan::UploadedPrimitive::make_skinned(device.view(), geometry, joints)}) {}

std::uint32_t SkinnedPrimitive::vertex_count() const { return m_primitive->layout().vertices; }
std::uint32_t SkinnedPrimitive::index_count() const { return m_primitive->layout().indices; }

Ptr<vulkan::Primitive> SkinnedPrimitive::vulkan_primitive() const { return m_primitive.get(); }

void DynamicPrimitive::Deleter::operator()(vulkan::HostPrimitive const* ptr) const { delete ptr; }

DynamicPrimitive::DynamicPrimitive(vulkan::Device& device) : m_primitive(new vulkan::HostPrimitive{device.view()}) {}

std::uint32_t DynamicPrimitive::vertex_count() const { return m_primitive->layout().vertices; }
std::uint32_t DynamicPrimitive::index_count() const { return m_primitive->layout().indices; }

void DynamicPrimitive::set_geometry(Geometry::Packed geometry) {
	assert(m_primitive);
	m_primitive->geometry = std::move(geometry);
}

Ptr<vulkan::Primitive> DynamicPrimitive::vulkan_primitive() const { return m_primitive.get(); }
} // namespace levk
