#pragma once
#include <djson/json.hpp>
#include <experiment/import_result.hpp>
#include <levk/graphics_device.hpp>
#include <levk/mesh_resources.hpp>

namespace levk::experiment {
struct TextureMetadata {
	Sampler sampler{};
	ColourSpace colour_space{ColourSpace::eSrgb};
	std::string image_path{};
};

template <typename Type>
using AssetUri = Id<Type, std::string>;

struct AssetMaterial {
	Rgba albedo{white_v};
	glm::vec3 emissive_factor{0.0f};
	float metallic{0.5f};
	float roughness{0.5f};
	AssetUri<Texture> base_colour{};
	AssetUri<Texture> roughness_metallic{};
	AssetUri<Texture> emissive{};
	PipelineState state{};
	float alpha_cutoff{};
	AlphaMode alpha_mode{AlphaMode::eOpaque};
	std::string shader{"shaders/lit.frag"};
	std::string name{};
};

void from_json(dj::Json const& json, AssetMaterial& out);
void to_json(dj::Json& out, AssetMaterial const& asset);

struct BinGeometry {
	struct Header {
		std::uint64_t hash{};
		std::uint64_t positions{};
		std::uint64_t indices{};
		std::uint64_t joints{};
		std::uint64_t weights{};
	};

	Geometry::Packed geometry{};
	std::vector<glm::uvec4> joints{};
	std::vector<glm::vec4> weights{};

	std::uint64_t compute_hash() const;
	bool write(char const* path) const;
	bool read(char const* path);
};

struct AssetMesh {
	enum class Type { eStatic, eSkinned };

	struct Primitive {
		AssetUri<BinGeometry> geometry{};
		AssetUri<Material> material{};
	};

	std::vector<Primitive> primitives{};
	AssetUri<Skeleton> skeleton{};
	std::string name{};
	Type type{};
};

void from_json(dj::Json const& json, AssetMesh& out);
void to_json(dj::Json& out, AssetMesh const& asset);

struct AssetSkeleton {
	Skeleton skeleton{};
};

struct ImportedMeshes {
	std::vector<AssetUri<AssetMesh>> meshes{};
	std::vector<AssetUri<AssetSkeleton>> skeletons{};
};

struct ResourceMetadata {
	template <typename Type, typename Meta>
	using Map = std::unordered_map<Id<Type>, Meta, std::hash<std::size_t>>;

	Map<Texture, TextureMetadata> textures{};
};

ImportResult import_gltf(char const* gltf_path, GraphicsDevice& device, MeshResources& out_resources, ResourceMetadata& out_meta);
ImportedMeshes import_gltf_meshes(char const* gltf_path, char const* dest_dir);

struct AssetLoader {
	GraphicsDevice& graphics_device;
	MeshResources& mesh_resources;

	Id<Texture> load_texture(char const* path, ColourSpace colour_space) const;
	Id<StaticMesh> load_static_mesh(char const* path) const;
};
} // namespace levk::experiment
