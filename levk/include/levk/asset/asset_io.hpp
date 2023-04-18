#pragma once
#include <djson/json.hpp>
#include <levk/asset/asset_type.hpp>
#include <levk/graphics/camera.hpp>
#include <levk/graphics/common.hpp>
#include <levk/graphics/geometry.hpp>
#include <levk/graphics/lights.hpp>
#include <levk/graphics/material.hpp>
#include <levk/graphics/skeleton.hpp>
#include <levk/graphics/texture.hpp>
#include <levk/level/level.hpp>
#include <levk/uri.hpp>
#include <variant>

namespace levk::asset {
Type get_type(std::string_view str);

void from_json(dj::Json const& json, Type& out);
void to_json(dj::Json& out, Type const& type);

void from_json(dj::Json const& json, Quad& out);
void to_json(dj::Json& out, Quad const& quad);

void from_json(dj::Json const& json, Cube& out);
void to_json(dj::Json& out, Cube const& cube);

void from_json(dj::Json const& json, Sphere& out);
void to_json(dj::Json& out, Sphere const& sphere);

void from_json(dj::Json const& json, ViewPlane& out);
void to_json(dj::Json& out, ViewPlane const& view_plane);

void from_json(dj::Json const& json, Camera& out);
void to_json(dj::Json& out, Camera const& camera);

void from_json(dj::Json const& json, Lights& out);
void to_json(dj::Json& out, Lights const& lights);

void from_json(dj::Json const& json, NodeTree& out);
void to_json(dj::Json& out, NodeTree const& node_tree);

void from_json(dj::Json const& json, Level& out);
void to_json(dj::Json& out, Level const& level);

struct Material {
	MaterialTextures textures{};
	Rgba albedo{white_v};
	glm::vec3 emissive_factor{0.0f};
	float metallic{0.5f};
	float roughness{0.5f};
	RenderMode render_mode{};
	float alpha_cutoff{};
	AlphaMode alpha_mode{AlphaMode::eOpaque};
	std::string vertex_shader{"shaders/lit.vert"};
	std::string fragment_shader{"shaders/lit.frag"};
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
	bool read(std::span<std::byte const> bytes);
};

struct BinSkeletalAnimation {
	enum class Type : std::uint8_t { eTranslate, eRotate, eScale };
	using Sampler = std::variant<TransformAnimation::Translate, TransformAnimation::Rotate, TransformAnimation::Scale>;

	struct Header {
		struct Sampler {
			Type type{};
			Interpolation interpolation{};
			std::uint64_t keyframes{};
		};

		std::uint64_t hash{};
		std::uint64_t samplers{};
		std::uint64_t target_joints{};
		std::uint64_t name_length{};
	};

	std::vector<Sampler> samplers{};
	std::vector<std::size_t> target_joints{};
	std::string name{};

	std::uint64_t compute_hash() const;
	bool write(char const* path) const;
	bool read(char const* path);
	bool read(std::span<std::byte const> bytes);
};

void from_json(dj::Json const& json, Skeleton& out);
void to_json(dj::Json& out, Skeleton const& asset);

struct Mesh3D {
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

void from_json(dj::Json const& json, Mesh3D& out);
void to_json(dj::Json& out, Mesh3D const& asset);
} // namespace levk::asset
