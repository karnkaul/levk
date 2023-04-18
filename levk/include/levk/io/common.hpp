#pragma once
#include <djson/json.hpp>
#include <glm/mat4x4.hpp>
#include <levk/graphics/common.hpp>
#include <levk/graphics/rgba.hpp>
#include <levk/rect.hpp>
#include <levk/transform.hpp>

namespace levk {
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

template <typename T = float>
void from_json(dj::Json const& json, Rect2D<T>& out, Rect2D<T> const& fallback = {}) {
	from_json(json["left_top"], out.lt, fallback.lt);
	from_json(json["right_bottom"], out.rb, fallback.rb);
}

template <typename T = float>
void to_json(dj::Json& out, Rect2D<T> const& rect) {
	to_json(out["left_top"], rect.lt);
	to_json(out["right_bottom"], rect.rb);
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
} // namespace levk
