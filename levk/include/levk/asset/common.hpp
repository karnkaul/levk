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

struct Skeleton {
	levk::Skeleton skeleton{};
};

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

void from_json(dj::Json const& json, Skeleton& out);
void to_json(dj::Json& out, Skeleton const& asset);
} // namespace levk::asset
