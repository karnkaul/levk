#include <levk/io/common.hpp>

namespace {
template <glm::length_t Dim>
void add_to(dj::Json& out, glm::vec<Dim, float> const& vec) {
	out.push_back(vec.x);
	if constexpr (Dim > 1) { out.push_back(vec.y); }
	if constexpr (Dim > 2) { out.push_back(vec.z); }
	if constexpr (Dim > 3) { out.push_back(vec.w); }
}

std::string to_hex(levk::Rgba const& rgba) {
	auto ret = std::string{"#"};
	fmt::format_to(std::back_inserter(ret), "{:02x}", rgba.channels.x);
	fmt::format_to(std::back_inserter(ret), "{:02x}", rgba.channels.y);
	fmt::format_to(std::back_inserter(ret), "{:02x}", rgba.channels.z);
	fmt::format_to(std::back_inserter(ret), "{:02x}", rgba.channels.w);
	return ret;
}

std::uint8_t to_channel(std::string_view const hex) {
	assert(hex.size() == 2);
	auto str = std::stringstream{};
	str << std::hex << hex;
	auto ret = std::uint32_t{};
	str >> ret;
	return static_cast<std::uint8_t>(ret);
}

levk::HdrRgba to_hdr_rgba(std::string_view hex, float intensity) {
	assert(!hex.empty());
	if (hex[0] == '#') { hex = hex.substr(1); }
	assert(hex.size() == 8);
	auto ret = levk::HdrRgba{};
	ret.channels.x = to_channel(hex.substr(0, 2));
	ret.channels.y = to_channel(hex.substr(2, 2));
	ret.channels.z = to_channel(hex.substr(4, 2));
	ret.channels.w = to_channel(hex.substr(6));
	ret.intensity = intensity;
	return ret;
}

using RenderMode = levk::RenderMode;

constexpr RenderMode::Type to_render_mode_type(std::string_view const str) {
	if (str == "fill") { return RenderMode::Type::eFill; }
	if (str == "line") { return RenderMode::Type::eLine; }
	if (str == "point") { return RenderMode::Type::ePoint; }
	return RenderMode::Type::eDefault;
}

constexpr std::string_view from(RenderMode::Type const type) {
	switch (type) {
	case RenderMode::Type::eFill: return "fill";
	case RenderMode::Type::eLine: return "line";
	case RenderMode::Type::ePoint: return "point";
	default: return "default";
	}
}
} // namespace

void levk::from_json(dj::Json const& json, glm::quat& out, glm::quat const& fallback) {
	auto vec = glm::vec4{};
	from_json(json, vec, {fallback.x, fallback.y, fallback.z, fallback.w});
	out = glm::quat{vec.w, vec.x, vec.y, vec.z};
}

void levk::to_json(dj::Json& out, glm::quat const& quat) {
	auto const vec = glm::vec4{quat.x, quat.y, quat.z, quat.w};
	to_json(out, vec);
}

void levk::from_json(dj::Json const& json, glm::mat4& out) {
	out[0] = glm_vec_from_json<4>(json, out[0]);
	out[1] = glm_vec_from_json<4>(json, out[1], 4);
	out[2] = glm_vec_from_json<4>(json, out[2], 8);
	out[3] = glm_vec_from_json<4>(json, out[3], 12);
}

void levk::to_json(dj::Json& out, glm::mat4 const& mat) {
	for (glm::length_t i = 0; i < 4; ++i) { add_to(out, mat[i]); }
}

void levk::to_json(dj::Json& out, Rgba const& rgba) {
	auto hdr = HdrRgba{rgba, 1.0f};
	return to_json(out, hdr);
}

void levk::from_json(dj::Json const& json, Rgba& out) {
	auto hdr = HdrRgba{};
	from_json(json, hdr);
	out = hdr;
}

void levk::to_json(dj::Json& out, HdrRgba const& rgba) {
	out["hex"] = to_hex(rgba);
	out["intensity"] = rgba.intensity;
}

void levk::from_json(dj::Json const& json, HdrRgba& out) { out = to_hdr_rgba(json["hex"].as_string(), json["intensity"].as<float>(1.0f)); }

void levk::from_json(dj::Json const& json, Transform& out) {
	auto mat = glm::mat4{1.0f};
	from_json(json, mat);
	out.decompose(mat);
}

void levk::to_json(dj::Json& out, Transform const& transform) { to_json(out, transform.matrix()); }

void levk::from_json(dj::Json const& json, RenderMode& out) {
	out.line_width = json["line_width"].as<float>(out.line_width);
	out.type = to_render_mode_type(json["type"].as_string());
	out.depth_test = json["depth_test"].as<bool>();
}

void levk::to_json(dj::Json& out, RenderMode const& render_mode) {
	out["line_width"] = render_mode.line_width;
	out["type"] = from(render_mode.type);
	out["depth_test"] = dj::Boolean{render_mode.depth_test};
}
