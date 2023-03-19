#pragma once
#include <levk/graphics/common.hpp>
#include <levk/graphics/image.hpp>
#include <levk/graphics/primitive.hpp>
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

std::unique_ptr<Primitive::Dynamic> gfx_make_dynamic(VulkanDevice const& device);
std::unique_ptr<Primitive::Static> gfx_make_static(VulkanDevice const& device, Geometry::Packed const& geometry);
std::unique_ptr<Primitive::Skinned> gfx_make_skinned(VulkanDevice const& device, Geometry::Packed const& geometry, MeshJoints const& joints);

Texture gfx_make_texture(VulkanDevice const& device, TextureCreateInfo create_info, Image::View image);
TextureSampler const& gfx_tex_sampler(VulkanTexture const& texture);
ColourSpace gfx_tex_colour_space(VulkanTexture const& texture);
Extent2D gfx_tex_extent(VulkanTexture const& texture);
std::uint32_t gfx_tex_mip_levels(VulkanTexture const& texture);
bool gfx_tex_resize_canvas(VulkanTexture& texture, Extent2D new_extent, Rgba background, glm::uvec2 top_left);
bool gfx_tex_write(VulkanTexture& texture, std::span<ImageWrite const> writes);

RenderStats gfx_render_stats(VulkanDevice const& device);
} // namespace levk
