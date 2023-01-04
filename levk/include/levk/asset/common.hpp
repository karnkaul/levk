#pragma once
#include <djson/json.hpp>
#include <levk/geometry.hpp>
#include <levk/graphics_common.hpp>
#include <levk/material.hpp>
#include <levk/skeleton.hpp>
#include <levk/skinned_mesh.hpp>
#include <levk/static_mesh.hpp>
#include <levk/texture.hpp>
#include <variant>

namespace levk::asset {
template <typename Type>
using Uri = Id<Type, std::string>;

void from_json(dj::Json const& json, Transform& out);
void to_json(dj::Json& out, Transform const& transform);

struct Material {
	Rgba albedo{white_v};
	glm::vec3 emissive_factor{0.0f};
	float metallic{0.5f};
	float roughness{0.5f};
	std::string base_colour{};
	std::string roughness_metallic{};
	std::string emissive{};
	RenderMode render_mode{};
	float alpha_cutoff{};
	AlphaMode alpha_mode{AlphaMode::eOpaque};
	std::string shader{"shaders/lit.frag"};
	std::string name{};
};

void from_json(dj::Json const& json, Material& out);
void to_json(dj::Json& out, Material const& asset);

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

struct SkeletalAnimation {
	TransformAnimation animation{};
	std::vector<std::size_t> target_joints{};
	std::string name{};
};

void from_json(dj::Json const& json, SkeletalAnimation& out);
void to_json(dj::Json& out, SkeletalAnimation const& asset);

struct Skeleton {
	using Joint = levk::Skeleton::Joint;

	std::vector<Joint> joints{};
	std::vector<Uri<SkeletalAnimation>> animations{};
	std::string name{};
};

void from_json(dj::Json const& json, Skeleton& out);
void to_json(dj::Json& out, Skeleton const& asset);

struct Mesh {
	enum class Type { eStatic, eSkinned };

	struct Primitive {
		Uri<BinGeometry> geometry{};
		Uri<Material> material{};
	};

	std::vector<Primitive> primitives{};
	std::vector<glm::mat4> inverse_bind_matrices{};
	Uri<Skeleton> skeleton{};
	std::string name{};
	Type type{};
};

void from_json(dj::Json const& json, Mesh& out);
void to_json(dj::Json& out, Mesh const& asset);
} // namespace levk::asset
