#pragma once
#include <levk/geometry.hpp>
#include <levk/graphics_common.hpp>
#include <levk/image.hpp>
#include <memory>

namespace levk {
struct TextureSampler;
struct TextureCreateInfo;
class Texture;
class MeshGeometry;
struct RenderInfo;
struct RenderStats;

struct VulkanDevice {
	struct Impl;

	VulkanDevice();

	struct Deleter {
		void operator()(Impl const* impl) const;
	};

	std::unique_ptr<Impl, Deleter> impl{};
};

struct VulkanMeshGeometry {
	class Impl;
	std::unique_ptr<Impl> impl{};
};

struct VulkanTexture {
	struct Impl;
	std::unique_ptr<Impl> impl{};
};

void gfx_create_device(VulkanDevice& out, GraphicsDeviceCreateInfo const& create_info);
void gfx_destroy_device(VulkanDevice& out);
GraphicsDeviceInfo const& gfx_info(VulkanDevice const& device);
bool gfx_set_vsync(VulkanDevice& out, Vsync::Type vsync);
bool gfx_set_render_scale(VulkanDevice& out, float scale);
void gfx_render(VulkanDevice const& device, RenderInfo const& info);

MeshGeometry gfx_make_static_mesh_geometry(VulkanDevice const& device, Geometry::Packed const& geometry);
MeshGeometry gfx_make_skinned_mesh_geometry(VulkanDevice const& device, Geometry::Packed const& geometry, MeshJoints const& joints);
MeshGeometry gfx_make_ui_mesh_geometry(VulkanDevice const& device, Geometry::Packed const& geometry);
std::uint32_t gfx_mesh_vertex_count(VulkanMeshGeometry const& mesh);
std::uint32_t gfx_mesh_index_count(VulkanMeshGeometry const& mesh);
bool gfx_mesh_has_joints(VulkanMeshGeometry const& mesh);

Texture gfx_make_texture(VulkanDevice const& device, TextureCreateInfo create_info, Image::View image);
TextureSampler const& gfx_tex_sampler(VulkanTexture const& texture);
ColourSpace gfx_tex_colour_space(VulkanTexture const& texture);
Extent2D gfx_tex_extent(VulkanTexture const& texture);
std::uint32_t gfx_tex_mip_levels(VulkanTexture const& texture);
bool gfx_tex_resize_canvas(VulkanTexture& texture, Extent2D new_extent, Rgba background, glm::uvec2 top_left);
bool gfx_tex_write(VulkanTexture& texture, std::span<ImageWrite const> writes);

RenderStats gfx_render_stats(VulkanDevice const& device);
} // namespace levk
