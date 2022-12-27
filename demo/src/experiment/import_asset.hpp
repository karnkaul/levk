#pragma once
#include <experiment/import_result.hpp>
#include <levk/graphics_device.hpp>
#include <levk/mesh_resources.hpp>

namespace levk::experiment {
struct TextureMetadata {
	Sampler sampler{};
	ColourSpace colour_space{ColourSpace::eSrgb};
	std::string image_path{};
};

struct ResourceMetadata {
	template <typename Type, typename Meta>
	using Map = std::unordered_map<Id<Type>, Meta, std::hash<std::size_t>>;

	Map<Texture, TextureMetadata> textures{};
};

ImportResult import_gltf(char const* gltf_path, GraphicsDevice& device, MeshResources& out_resources, ResourceMetadata& out_meta);
} // namespace levk::experiment
