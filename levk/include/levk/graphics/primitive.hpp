#pragma once
#include <levk/graphics/geometry.hpp>
#include <levk/graphics/shader_code.hpp>
#include <levk/uri.hpp>
#include <levk/util/ptr.hpp>

namespace levk {
enum class Topology : std::uint8_t { ePointList, eLineList, eLineStrip, eTriangleList, eTriangleStrip, eTriangleFan };

namespace vulkan {
struct Device;
struct Primitive;
class UploadedPrimitive;
class HostPrimitive;
} // namespace vulkan

class Primitive {
  public:
	virtual ~Primitive() = default;

	virtual std::uint32_t vertex_count() const = 0;
	virtual std::uint32_t index_count() const = 0;

	virtual Ptr<vulkan::Primitive> vulkan_primitive() const = 0;

	Topology topology{Topology::eTriangleList};
};

class StaticPrimitive : public Primitive {
  public:
	StaticPrimitive(vulkan::Device& device, Geometry::Packed const& geometry);

	std::uint32_t vertex_count() const final;
	std::uint32_t index_count() const final;
	Ptr<vulkan::Primitive> vulkan_primitive() const final;

  private:
	struct Deleter {
		void operator()(vulkan::UploadedPrimitive const* ptr) const;
	};

	std::unique_ptr<vulkan::UploadedPrimitive, Deleter> m_primitive{};
};

class SkinnedPrimitive : public Primitive {
  public:
	SkinnedPrimitive(vulkan::Device& device, Geometry::Packed const& geometry, MeshJoints const& joints);

	std::uint32_t vertex_count() const final;
	std::uint32_t index_count() const final;
	Ptr<vulkan::Primitive> vulkan_primitive() const final;

  private:
	struct Deleter {
		void operator()(vulkan::UploadedPrimitive const* ptr) const;
	};

	std::unique_ptr<vulkan::UploadedPrimitive, Deleter> m_primitive{};
};

class DynamicPrimitive : public Primitive {
  public:
	DynamicPrimitive(vulkan::Device& device);

	void set_geometry(Geometry::Packed geometry);

	std::uint32_t vertex_count() const final;
	std::uint32_t index_count() const final;
	Ptr<vulkan::Primitive> vulkan_primitive() const final;

  private:
	struct Deleter {
		void operator()(vulkan::HostPrimitive const* ptr) const;
	};

	std::unique_ptr<vulkan::HostPrimitive, Deleter> m_primitive{};
};
} // namespace levk
