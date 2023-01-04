#include <fmt/format.h>
#include <glm/gtx/matrix_decompose.hpp>
#include <levk/asset/common.hpp>
#include <levk/util/visitor.hpp>
#include <sstream>

namespace levk {
namespace {
constexpr Interpolation to_interpolation(std::string_view const str) {
	if (str == "step") { return Interpolation::eStep; }
	return Interpolation::eLinear;
}

constexpr std::string_view from(Interpolation const i) {
	switch (i) {
	case Interpolation::eStep: return "step";
	default: return "linear";
	}
}

constexpr RenderMode::Type to_render_mode(std::string_view const str) {
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

constexpr AlphaMode to_alpha_mode(std::string_view const str) {
	if (str == "blend") { return AlphaMode::eBlend; }
	if (str == "mask") { return AlphaMode::eMask; }
	return AlphaMode::eOpaque;
}

constexpr std::string_view from(AlphaMode const mode) {
	switch (mode) {
	case AlphaMode::eBlend: return "blend";
	case AlphaMode::eMask: return "mask";
	default: return "opaque";
	}
}

std::string to_hex(Rgba const& rgba) {
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

Rgba to_rgba(std::string_view hex, float intensity) {
	assert(!hex.empty());
	if (hex[0] == '#') { hex = hex.substr(1); }
	assert(hex.size() == 8);
	auto ret = Rgba{};
	ret.channels.x = to_channel(hex.substr(0, 2));
	ret.channels.y = to_channel(hex.substr(2, 2));
	ret.channels.z = to_channel(hex.substr(4, 2));
	ret.channels.w = to_channel(hex.substr(6));
	ret.intensity = intensity;
	return ret;
}

dj::Json make_json(Rgba const& rgba) {
	auto ret = dj::Json{};
	ret["hex"] = to_hex(rgba);
	ret["intensity"] = rgba.intensity;
	return ret;
}

Rgba make_rgba(dj::Json const& json) { return to_rgba(json["hex"].as_string(), json["intensity"].as<float>(1.0f)); }

RenderMode make_render_mode(dj::Json const& json) {
	auto ret = RenderMode{};
	ret.line_width = json["line_width"].as<float>(ret.line_width);
	ret.type = to_render_mode(json["type"].as_string());
	ret.depth_test = json["depth_test"].as<bool>();
	return ret;
}

dj::Json make_json(RenderMode const& render_mode) {
	auto ret = dj::Json{};
	ret["line_width"] = render_mode.line_width;
	ret["type"] = from(render_mode.type);
	ret["depth_test"] = dj::Boolean{render_mode.depth_test};
	return ret;
}

template <glm::length_t Dim>
void add_to(dj::Json& out, glm::vec<Dim, float> const& vec) {
	out.push_back(vec.x);
	if constexpr (Dim > 1) { out.push_back(vec.y); }
	if constexpr (Dim > 2) { out.push_back(vec.z); }
	if constexpr (Dim > 3) { out.push_back(vec.w); }
}

template <glm::length_t Dim, typename T = float>
glm::vec<Dim, T> glm_vec_from_json(dj::Json const& json, glm::vec<Dim, T> const& fallback = {}, std::size_t offset = 0) {
	auto ret = glm::vec<Dim, T>{};
	ret.x = json[offset + 0].as<T>(fallback.x);
	if constexpr (Dim > 1) { ret.y = json[offset + 1].as<T>(fallback.y); }
	if constexpr (Dim > 2) { ret.z = json[offset + 2].as<T>(fallback.z); }
	if constexpr (Dim > 3) { ret.w = json[offset + 3].as<T>(fallback.w); }
	return ret;
}

glm::mat4 glm_mat_from_json(dj::Json const& json) {
	auto ret = glm::mat4{};
	ret[0] = glm_vec_from_json<4>(json);
	ret[1] = glm_vec_from_json<4>(json, {}, 4);
	ret[2] = glm_vec_from_json<4>(json, {}, 8);
	ret[3] = glm_vec_from_json<4>(json, {}, 12);
	return ret;
}

dj::Json make_json(glm::vec3 const& vec3) {
	auto ret = dj::Json{};
	add_to(ret, vec3);
	return ret;
}

dj::Json make_json(glm::quat const& quat) {
	auto ret = dj::Json{};
	auto vec4 = glm::vec4{quat.x, quat.y, quat.z, quat.w};
	add_to(ret, vec4);
	return ret;
}

dj::Json make_json(glm::mat4 const& mat) {
	auto ret = dj::Json{};
	for (glm::length_t i = 0; i < 4; ++i) { add_to(ret, mat[i]); }
	return ret;
}

glm::quat make_quat(dj::Json const& json) {
	auto ret = glm::quat{};
	assert(json.array_view().size() >= 4);
	ret.x = json[0].as<float>();
	ret.y = json[1].as<float>();
	ret.z = json[2].as<float>();
	ret.w = json[3].as<float>();
	return ret;
}

template <typename... T>
[[maybe_unused]] constexpr bool false_v = false;

template <typename T>
T glm_from_json(dj::Json const& json) {
	if constexpr (std::same_as<T, glm::quat>) {
		return make_quat(json);
	} else if constexpr (std::same_as<T, glm::vec3>) {
		return glm_vec_from_json<3>(json);
	} else {
		static_assert(false_v<T>, "Type not implemented");
	}
}

template <typename T>
dj::Json make_json(std::vector<typename Interpolator<T>::Keyframe> const& keyframes) {
	auto ret = dj::Json{};
	for (auto const& in_kf : keyframes) {
		auto out_kf = dj::Json{};
		out_kf["timestamp"] = in_kf.timestamp.count();
		out_kf["value"] = make_json(in_kf.value);
		ret.push_back(std::move(out_kf));
	}
	return ret;
}

template <typename T>
std::vector<typename Interpolator<T>::Keyframe> make_keyframes(dj::Json const& json) {
	auto ret = std::vector<typename Interpolator<T>::Keyframe>{};
	for (auto const& in_kf : json.array_view()) {
		auto out_kf = typename Interpolator<T>::Keyframe{};
		out_kf.timestamp = Time{in_kf["timestamp"].as<float>()};
		out_kf.value = glm_from_json<T>(in_kf["value"]);
		ret.push_back(std::move(out_kf));
	}
	return ret;
}

Skeleton::Sampler make_skeleton_sampler(dj::Json const& json) {
	auto ret = Skeleton::Sampler{};
	auto const type = json["type"].as_string();
	if (type == "translate") {
		auto sampler = TransformAnimator::Translate{};
		sampler.keyframes = make_keyframes<glm::vec3>(json["keyframes"]);
		sampler.interpolation = to_interpolation(json["interpolation"].as_string());
		ret = std::move(sampler);
	} else if (type == "rotate") {
		auto sampler = TransformAnimator::Rotate{};
		sampler.keyframes = make_keyframes<glm::quat>(json["keyframes"]);
		sampler.interpolation = to_interpolation(json["interpolation"].as_string());
		ret = std::move(sampler);
	} else if (type == "scale") {
		auto sampler = TransformAnimator::Scale{};
		sampler.keyframes = make_keyframes<glm::vec3>(json["keyframes"]);
		sampler.interpolation = to_interpolation(json["interpolation"].as_string());
		ret = std::move(sampler);
	}
	return ret;
}

dj::Json make_json(Skeleton::Sampler const& asset) {
	auto ret = dj::Json{};
	ret["asset_type"] = "animation_sampler";
	auto visitor = Visitor{
		[&](TransformAnimator::Translate const& t) {
			ret["type"] = "translate";
			ret["interpolation"] = from(t.interpolation);
			ret["keyframes"] = make_json<glm::vec3>(t.keyframes);
		},
		[&](TransformAnimator::Rotate const& r) {
			ret["type"] = "rotate";
			ret["interpolation"] = from(r.interpolation);
			ret["keyframes"] = make_json<glm::quat>(r.keyframes);
		},
		[&](TransformAnimator::Scale const s) {
			ret["type"] = "scale";
			ret["interpolation"] = from(s.interpolation);
			ret["keyframes"] = make_json<glm::vec3>(s.keyframes);
		},
	};
	std::visit(visitor, asset);
	return ret;
}

JointAnimation::Sampler make_animation_sampler(dj::Json const& json) {
	auto ret = JointAnimation::Sampler{};
	auto const type = json["type"].as_string();
	if (type == "translate") {
		auto sampler = JointAnimation::Translate{};
		sampler.keyframes = make_keyframes<glm::vec3>(json["keyframes"]);
		sampler.interpolation = to_interpolation(json["interpolation"].as_string());
		ret.storage = std::move(sampler);
	} else if (type == "rotate") {
		auto sampler = JointAnimation::Rotate{};
		sampler.keyframes = make_keyframes<glm::quat>(json["keyframes"]);
		sampler.interpolation = to_interpolation(json["interpolation"].as_string());
		ret.storage = std::move(sampler);
	} else if (type == "scale") {
		auto sampler = JointAnimation::Scale{};
		sampler.keyframes = make_keyframes<glm::vec3>(json["keyframes"]);
		sampler.interpolation = to_interpolation(json["interpolation"].as_string());
		ret.storage = std::move(sampler);
	}
	return ret;
}

dj::Json make_json(JointAnimation::Sampler const& asset) {
	auto ret = dj::Json{};
	ret["asset_type"] = "animation_sampler";
	auto visitor = Visitor{
		[&](JointAnimation::Translate const& t) {
			ret["type"] = "translate";
			ret["interpolation"] = from(t.interpolation);
			ret["keyframes"] = make_json<glm::vec3>(t.keyframes);
		},
		[&](JointAnimation::Rotate const& r) {
			ret["type"] = "rotate";
			ret["interpolation"] = from(r.interpolation);
			ret["keyframes"] = make_json<glm::quat>(r.keyframes);
		},
		[&](JointAnimation::Scale const s) {
			ret["type"] = "scale";
			ret["interpolation"] = from(s.interpolation);
			ret["keyframes"] = make_json<glm::vec3>(s.keyframes);
		},
	};
	std::visit(visitor, asset.storage);
	return ret;
}
} // namespace

void asset::from_json(dj::Json const& json, Transform& out) {
	auto const mat = glm_mat_from_json(json);
	glm::vec3 scale{1.0f}, pos{}, skew{};
	glm::vec4 persp{};
	glm::quat orn{quat_identity_v};
	glm::decompose(mat, scale, orn, pos, skew, persp);
	out.set_position(pos);
	out.set_orientation(orn);
	out.set_scale(scale);
}

void asset::to_json(dj::Json& out, Transform const& transform) { out = make_json(transform.matrix()); }

void asset::from_json(dj::Json const& json, asset::Material& out) {
	assert(json["asset_type"].as_string() == "material");
	out.albedo = make_rgba(json["albedo"]);
	out.emissive_factor = glm_vec_from_json<3>(json["emissive_factor"], out.emissive_factor);
	out.metallic = json["metallic"].as<float>(out.metallic);
	out.roughness = json["roughness"].as<float>(out.roughness);
	out.base_colour = std::string{json["base_colour"].as_string()};
	out.roughness_metallic = std::string{json["roughness_metallic"].as_string()};
	out.emissive = std::string{json["emissive"].as_string()};
	out.render_mode = make_render_mode(json["render_mode"]);
	out.alpha_cutoff = json["alpha_cutoff"].as<float>(out.alpha_cutoff);
	out.alpha_mode = to_alpha_mode(json["alpha_mode"].as_string());
	out.shader = json["shader"].as_string(out.shader);
	out.name = json["name"].as_string();
}

void asset::to_json(dj::Json& out, asset::Material const& asset) {
	out["asset_type"] = "material";
	out["albedo"] = make_json(asset.albedo);
	out["emissive_factor"] = make_json(asset.emissive_factor);
	out["metallic"] = asset.metallic;
	out["roughness"] = asset.roughness;
	if (!asset.base_colour.empty()) { out["base_colour"] = asset.base_colour; }
	if (!asset.roughness_metallic.empty()) { out["roughness_metallic"] = asset.roughness_metallic; }
	if (!asset.emissive.empty()) { out["emissive"] = asset.emissive; }
	out["render_mode"] = make_json(asset.render_mode);
	out["alpha_cutoff"] = asset.alpha_cutoff;
	out["alpha_mode"] = from(asset.alpha_mode);
	out["shader"] = asset.shader;
	out["name"] = asset.name;
}

void asset::from_json(dj::Json const& json, Mesh& out) {
	assert(json["asset_type"].as_string() == "mesh");
	out.type = json["type"].as_string() == "skinned" ? Mesh::Type::eSkinned : Mesh::Type::eStatic;
	for (auto const& in_primitive : json["primitives"].array_view()) {
		auto primitive = Mesh::Primitive{};
		primitive.geometry = std::string{in_primitive["geometry"].as_string()};
		primitive.material = std::string{in_primitive["material"].as_string()};
		out.primitives.push_back(std::move(primitive));
	}
	out.skeleton = std::string{json["skeleton"].as_string()};
	for (auto const& in_ibm : json["inverse_bind_matrices"].array_view()) { out.inverse_bind_matrices.push_back(glm_mat_from_json(in_ibm)); }
	out.name = json["name"].as_string();
}

void asset::to_json(dj::Json& out, Mesh const& asset) {
	out["asset_type"] = "mesh";
	out["type"] = asset.type == Mesh::Type::eSkinned ? "skinned" : "static";
	if (!asset.primitives.empty()) {
		auto primitives = dj::Json{};
		for (auto const& in_primitive : asset.primitives) {
			auto out_primitive = dj::Json{};
			out_primitive["geometry"] = in_primitive.geometry.value();
			if (!in_primitive.material.value().empty()) { out_primitive["material"] = in_primitive.material.value(); }
			primitives.push_back(std::move(out_primitive));
		}
		out["primitives"] = std::move(primitives);
	}
	if (!asset.skeleton.value().empty()) {
		out["skeleton"] = asset.skeleton.value();
		assert(!asset.inverse_bind_matrices.empty());
		auto ibm = dj::Json{};
		for (auto const& in_ibm : asset.inverse_bind_matrices) { ibm.push_back(make_json(in_ibm)); }
		out["inverse_bind_matrices"] = std::move(ibm);
	}
	out["name"] = asset.name;
}

void asset::from_json(dj::Json const& json, Skeleton& out) {
	assert(json["asset_type"].as_string() == "skeleton");
	out.skeleton.name = json["name"].as_string();
	for (auto const& in_joint : json["joints"].array_view()) {
		auto& out_joint = out.skeleton.joints.emplace_back();
		out_joint.name = in_joint["name"].as_string();
		from_json(in_joint["transform"], out_joint.transform);
		out_joint.self = in_joint["self"].as<std::size_t>();
		if (auto const& parent = in_joint["parent"]; parent.is_number()) { out_joint.parent = parent.as<std::size_t>(); }
		for (auto const& child : in_joint["children"].array_view()) { out_joint.children.push_back(child.as<std::size_t>()); }
	}
	for (auto const& in_clip : json["clips"].array_view()) {
		auto& out_clip = out.skeleton.clips.emplace_back();
		out_clip.name = in_clip["name"].as_string();
		for (auto const& in_channel : in_clip["channels"].array_view()) {
			auto& out_channel = out_clip.channels.emplace_back();
			out_channel.target = in_channel["target"].as<std::size_t>();
			out_channel.sampler = make_skeleton_sampler(in_channel["sampler"]);
		}
	}
	for (auto const& in_source : json["animation_sources"].array_view()) {
		auto& out_source = out.skeleton.animation_sources.emplace_back();
		out_source.animation.name = in_source["name"].as_string();
		for (auto const& in_sampler : in_source["samplers"].array_view()) {
			auto& out_sampler = out_source.animation.samplers.emplace_back();
			out_sampler = make_animation_sampler(in_sampler);
		}
		for (auto const& in_target : in_source["target_joints"].array_view()) { out_source.target_joints.push_back(in_target.as<std::size_t>()); }
	}
}

void asset::to_json(dj::Json& out, Skeleton const& asset) {
	out["asset_type"] = "skeleton";
	out["name"] = asset.skeleton.name;
	auto joints = dj::Json{};
	for (auto const& in_joint : asset.skeleton.joints) {
		auto out_joint = dj::Json{};
		out_joint["name"] = in_joint.name;
		to_json(out_joint["transform"], in_joint.transform);
		out_joint["self"] = in_joint.self;
		if (in_joint.parent) { out_joint["parent"] = *in_joint.parent; }
		auto children = dj::Json{};
		for (auto const child : in_joint.children) { children.push_back(child); }
		out_joint["children"] = std::move(children);
		joints.push_back(std::move(out_joint));
	}
	out["joints"] = std::move(joints);
	auto clips = dj::Json{};
	for (auto const& in_clip : asset.skeleton.clips) {
		auto out_clip = dj::Json{};
		out_clip["name"] = in_clip.name;
		auto channels = dj::Json{};
		for (auto const& in_channel : in_clip.channels) {
			auto out_channel = dj::Json{};
			out_channel["target"] = in_channel.target;
			out_channel["sampler"] = make_json(in_channel.sampler);
			channels.push_back(std::move(out_channel));
		}
		out_clip["channels"] = std::move(channels);
		clips.push_back(std::move(out_clip));
	}
	out["clips"] = std::move(clips);
	auto out_animation_sources = dj::Json{};
	for (auto const& in_source : asset.skeleton.animation_sources) {
		auto out_source = dj::Json{};
		out_source["name"] = in_source.animation.name;
		auto out_samplers = dj::Json{};
		for (auto const& in_sampler : in_source.animation.samplers) { out_samplers.push_back(make_json(in_sampler)); }
		out_source["samplers"] = std::move(out_samplers);
		auto out_targets = dj::Json{};
		for (auto const in_target : in_source.target_joints) { out_targets.push_back(in_target); }
		out_source["target_joints"] = std::move(out_targets);
		out_animation_sources.push_back(std::move(out_source));
	}
	out["animation_sources"] = std::move(out_animation_sources);
}
} // namespace levk
