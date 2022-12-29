#pragma once
#include <djson/json.hpp>
#include <gltf2cpp/gltf2cpp.hpp>
#include <levk/graphics_device.hpp>
#include <levk/mesh_resources.hpp>
#include <levk/util/logger.hpp>

namespace levk {
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

void from_json(dj::Json const& json, AssetSkeleton& out);
void to_json(dj::Json& out, AssetSkeleton const& asset);

struct ImportedMeshes {
	std::vector<AssetUri<AssetMesh>> meshes{};
	std::vector<AssetUri<AssetSkeleton>> skeletons{};
};

struct GltfAssetView {
	struct List;

	std::string gltf_name{};
	std::string asset_name{};
	std::size_t index{};
};

struct GltfAssetView::List {
	std::vector<GltfAssetView> static_meshes{};
	std::vector<GltfAssetView> skinned_meshes{};
	std::vector<GltfAssetView> skeletons{};
};

ImportedMeshes import_gltf_meshes(char const* gltf_path, char const* dest_dir, logger::Dispatch const& import_logger = {});

struct GltfAssetImporter {
	logger::Dispatch const& import_logger;
	gltf2cpp::Root root{};
	std::string src_dir{};
	std::string dest_dir{};

	struct List : GltfAssetView::List {
		logger::Dispatch const& import_logger;
		std::string gltf_path{};

		List(logger::Dispatch const& import_logger) : import_logger(import_logger) {}

		GltfAssetImporter importer(std::string dest_dir) const;

		explicit operator bool() const { return !gltf_path.empty(); }
	};

	static List peek(logger::Dispatch const& import_logger, std::string gltf_path);

	std::variant<AssetUri<StaticMesh>, AssetUri<SkinnedMesh>> import_mesh(GltfAssetView const& mesh) const;

	explicit operator bool() const { return !dest_dir.empty() && root; }
};

struct AssetLoader {
	GraphicsDevice& graphics_device;
	MeshResources& mesh_resources;
	logger::Dispatch import_logger{};

	Id<Texture> load_texture(char const* path, ColourSpace colour_space) const;
	Id<StaticMesh> try_load_static_mesh(char const* path) const;
	Id<StaticMesh> load_static_mesh(char const* path, dj::Json const& json) const;
	Id<Skeleton> load_skeleton(char const* path) const;
	Id<SkinnedMesh> try_load_skinned_mesh(char const* path) const;
	Id<SkinnedMesh> load_skinned_mesh(char const* path, dj::Json const& json) const;
	std::variant<std::monostate, Id<StaticMesh>, Id<SkinnedMesh>> try_load_mesh(char const* path) const;
};
} // namespace levk
