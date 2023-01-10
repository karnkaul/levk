#pragma once
#include <levk/geometry.hpp>
#include <levk/graphics_common.hpp>
#include <levk/image.hpp>
#include <memory>

namespace levk {
struct Reader;

struct TextureSampler;
struct TextureCreateInfo;
class Texture;
class MeshGeometry;

struct VulkanDevice {
	struct Impl;

	VulkanDevice(Reader& reader);
	VulkanDevice(VulkanDevice&&) noexcept;
	VulkanDevice& operator=(VulkanDevice&&) noexcept;
	~VulkanDevice() noexcept;

	std::unique_ptr<Impl> impl{};
};

struct VulkanMeshGeometry {
	class Impl;

	VulkanMeshGeometry() noexcept;
	VulkanMeshGeometry(VulkanMeshGeometry&&) noexcept;
	VulkanMeshGeometry& operator=(VulkanMeshGeometry&&) noexcept;
	~VulkanMeshGeometry() noexcept;

	std::unique_ptr<Impl> impl{};
};

struct VulkanTexture {
	class Impl;

	VulkanTexture() noexcept;
	VulkanTexture(VulkanTexture&&) noexcept;
	VulkanTexture& operator=(VulkanTexture&&) noexcept;
	~VulkanTexture() noexcept;

	std::unique_ptr<Impl> impl{};
};

void gfx_create_device(VulkanDevice& out, GraphicsDeviceCreateInfo const& create_info);
void gfx_destroy_device(VulkanDevice& out);
GraphicsDeviceInfo const& gfx_info(VulkanDevice const& device);
bool gfx_set_vsync(VulkanDevice& out, Vsync::Type vsync);
bool gfx_set_render_scale(VulkanDevice& out, float scale);
void gfx_render(VulkanDevice& out, RenderInfo const& info);

MeshGeometry gfx_make_mesh_geometry(VulkanDevice const& device, Geometry::Packed const& geometry, MeshJoints const& joints);
std::uint32_t gfx_mesh_vertex_count(VulkanMeshGeometry const& mesh);
std::uint32_t gfx_mesh_index_count(VulkanMeshGeometry const& mesh);
bool gfx_mesh_has_joints(VulkanMeshGeometry const& mesh);

Texture gfx_make_texture(VulkanDevice const& device, TextureCreateInfo create_info, Image::View image);
TextureSampler const& gfx_tex_sampler(VulkanTexture const& texture);
ColourSpace gfx_tex_colour_space(VulkanTexture const& texture);
Extent2D gfx_tex_extent(VulkanTexture const& texture);
std::uint32_t gfx_tex_mip_levels(VulkanTexture const& texture);

void gfx_render(VulkanDevice& out, StaticMeshRenderInfo const& info);
void gfx_render(VulkanDevice& out, SkinnedMeshRenderInfo const& info);
RenderStats gfx_render_stats(VulkanDevice const& device);
} // namespace levk
