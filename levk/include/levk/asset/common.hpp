#pragma once
#include <djson/json.hpp>
#include <levk/geometry.hpp>
#include <levk/graphics_common.hpp>
#include <levk/material.hpp>
#include <levk/skeleton.hpp>
#include <levk/skinned_mesh.hpp>
#include <levk/static_mesh.hpp>
#include <levk/texture.hpp>
#include <levk/uri.hpp>
#include <variant>

namespace levk::asset {
template <glm::length_t Dim, typename T = float>
glm::vec<Dim, T> glm_vec_from_json(dj::Json const& json, glm::vec<Dim, T> const& fallback = {}, std::size_t offset = 0) {
	auto ret = glm::vec<Dim, T>{};
	ret.x = json[offset + 0].as<T>(fallback.x);
	if constexpr (Dim > 1) { ret.y = json[offset + 1].as<T>(fallback.y); }
	if constexpr (Dim > 2) { ret.z = json[offset + 2].as<T>(fallback.z); }
	if constexpr (Dim > 3) { ret.w = json[offset + 3].as<T>(fallback.w); }
	return ret;
}

template <glm::length_t Dim, typename T = float>
void from_json(dj::Json const& json, glm::vec<Dim, T>& out, glm::vec<Dim, T> const& fallback = {}) {
	out = glm_vec_from_json(json, fallback);
}

template <glm::length_t Dim, typename T = float>
void to_json(dj::Json& out, glm::vec<Dim, T> const& vec) {
	out.push_back(vec.x);
	if constexpr (Dim > 1) { out.push_back(vec.y); }
	if constexpr (Dim > 2) { out.push_back(vec.z); }
	if constexpr (Dim > 3) { out.push_back(vec.w); }
}

void from_json(dj::Json const& json, glm::quat& out, glm::quat const& fallback = glm::identity<glm::quat>());
void to_json(dj::Json& out, glm::quat const& quat);

void from_json(dj::Json const& json, glm::mat4& out);
void to_json(dj::Json& out, glm::mat4 const& mat);

void from_json(dj::Json const& json, Rgba& out);
void to_json(dj::Json& out, Rgba const& rgba);

void from_json(dj::Json const& json, HdrRgba& out);
void to_json(dj::Json& out, HdrRgba const& rgba);

void from_json(dj::Json const& json, Transform& out);
void to_json(dj::Json& out, Transform const& transform);

void from_json(dj::Json const& json, RenderMode& out);
void to_json(dj::Json& out, RenderMode const& render_mode);

struct Material {
	MaterialTextures textures{};
	Rgba albedo{white_v};
	glm::vec3 emissive_factor{0.0f};
	float metallic{0.5f};
	float roughness{0.5f};
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

struct Skeleton {
	using Joint = levk::Skeleton::Joint;

	std::vector<Joint> joints{};
	std::vector<Uri<BinSkeletalAnimation>> animations{};
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
