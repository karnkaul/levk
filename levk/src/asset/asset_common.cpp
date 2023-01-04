#include <fmt/format.h>
#include <glm/gtx/matrix_decompose.hpp>
#include <levk/asset/common.hpp>
#include <levk/util/binary_file.hpp>
#include <levk/util/hash_combine.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/visitor.hpp>
#include <sstream>

namespace levk {
namespace {
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

dj::Json make_json(glm::mat4 const& mat) {
	auto ret = dj::Json{};
	for (glm::length_t i = 0; i < 4; ++i) { add_to(ret, mat[i]); }
	return ret;
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

template <typename T>
T get_sampler(asset::BinSkeletalAnimation::Header::Sampler const& header, BinaryInFile& file) {
	auto ret = T{};
	ret.interpolation = header.interpolation;
	ret.keyframes.resize(static_cast<std::size_t>(header.keyframes));
	file.read(std::span{ret.keyframes});
	return ret;
}
} // namespace

namespace asset {
std::uint64_t BinGeometry::compute_hash() const {
	auto ret = std::size_t{};
	for (auto const& position : geometry.positions) { hash_combine(ret, position.x, position.y, position.z); }
	hash_combine(ret, geometry.rgbs.size(), geometry.normals.size(), geometry.uvs.size(), geometry.indices.size(), joints.size());
	return static_cast<std::uint64_t>(ret);
}

bool BinGeometry::write(char const* path) const {
	auto file = BinaryOutFile{path};
	if (!file) { return false; }
	auto const header = Header{
		.hash = compute_hash(),
		.positions = geometry.positions.size(),
		.indices = geometry.indices.size(),
		.joints = joints.size(),
		.weights = weights.size(),
	};
	file.write(std::span{&header, 1});
	file.write(std::span{geometry.positions});
	file.write(std::span{geometry.rgbs});
	file.write(std::span{geometry.normals});
	file.write(std::span{geometry.uvs});
	file.write(std::span{geometry.indices});
	if (!joints.empty()) {
		file.write(std::span{joints});
		file.write(std::span{weights});
	}
	return true;
}

bool BinGeometry::read(char const* path) {
	auto file = BinaryInFile{path};
	if (!file) { return false; }

	auto in = BinGeometry{};
	auto header = Header{};
	file.read(header);

	in.geometry.positions.resize(static_cast<std::size_t>(header.positions));
	file.read(std::span{in.geometry.positions});

	in.geometry.rgbs.resize(static_cast<std::size_t>(header.positions));
	file.read(std::span{in.geometry.rgbs});

	in.geometry.normals.resize(static_cast<std::size_t>(header.positions));
	file.read(std::span{in.geometry.normals});

	in.geometry.uvs.resize(static_cast<std::size_t>(header.positions));
	file.read(std::span{in.geometry.uvs});

	if (header.indices) {
		in.geometry.indices.resize(static_cast<std::size_t>(header.indices));
		file.read(std::span{in.geometry.indices});
	}

	if (header.joints) {
		in.joints.resize(static_cast<std::size_t>(header.joints));
		file.read(std::span{in.joints});

		assert(header.weights == header.joints);
		in.weights.resize(static_cast<std::size_t>(header.weights));
		file.read(std::span{in.weights});
	}

	if (in.compute_hash() != header.hash) { return false; }

	*this = std::move(in);
	return true;
}

template <typename T>
constexpr bool has_w_v = requires(T t) { t.w; };

std::uint64_t BinSkeletalAnimation::compute_hash() const {
	auto ret = std::size_t{};
	auto combine_sampler = [&ret](auto const& sampler) {
		hash_combine(ret, sampler.interpolation);
		for (auto const& keyframe : sampler.keyframes) {
			hash_combine(ret, keyframe.timestamp.count(), keyframe.value.x, keyframe.value.x, keyframe.value.z);
			if constexpr (has_w_v<decltype(keyframe.value)>) { hash_combine(ret, keyframe.value.w); }
		}
	};
	for (auto const& sampler : samplers) { std::visit(combine_sampler, sampler); }
	for (auto const joint : target_joints) { hash_combine(ret, joint); }
	hash_combine(ret, name);
	return ret;
}

bool BinSkeletalAnimation::write(char const* path) const {
	auto file = BinaryOutFile{path};
	if (!file) { return false; }
	auto const header = Header{
		.hash = compute_hash(),
		.samplers = static_cast<std::uint64_t>(samplers.size()),
		.target_joints = static_cast<std::uint64_t>(target_joints.size()),
		.name_length = static_cast<std::uint64_t>(name.size()),
	};
	file.write(std::span{&header, 1u});
	auto write_sampler = [&file](auto const& sampler, Type const type) {
		auto const header = Header::Sampler{
			.type = type,
			.interpolation = sampler.interpolation,
			.keyframes = static_cast<std::uint64_t>(sampler.keyframes.size()),
		};
		file.write(std::span{&header, 1u});
		file.write(std::span{sampler.keyframes});
	};
	auto visitor = Visitor{
		[&](TransformAnimation::Translate const& translate) { write_sampler(translate, Type::eTranslate); },
		[&](TransformAnimation::Rotate const& rotate) { write_sampler(rotate, Type::eRotate); },
		[&](TransformAnimation::Scale const& scale) { write_sampler(scale, Type::eScale); },
	};
	for (auto const& sampler : samplers) { std::visit(visitor, sampler); }

	file.write(std::span{target_joints});
	file.write(std::span{name.data(), name.size()});

	return true;
}

bool BinSkeletalAnimation::read(char const* path) {
	auto file = BinaryInFile{path};
	if (!file) { return false; }

	auto in = BinSkeletalAnimation{};
	auto header = Header{};
	file.read(header);

	for (std::uint64_t i = 0; i < header.samplers; ++i) {
		auto sampler_header = Header::Sampler{};
		file.read(sampler_header);
		auto& out_sampler = in.samplers.emplace_back();
		switch (sampler_header.type) {
		case Type::eTranslate: out_sampler = get_sampler<TransformAnimation::Translate>(sampler_header, file); break;
		case Type::eRotate: out_sampler = get_sampler<TransformAnimation::Rotate>(sampler_header, file); break;
		case Type::eScale: out_sampler = get_sampler<TransformAnimation::Scale>(sampler_header, file); break;
		default: logger::error("[BinSkeletonAnimation] Unrecognized type: [{}]", static_cast<int>(sampler_header.type)); break;
		}
	}

	in.target_joints.resize(static_cast<std::size_t>(header.target_joints));
	file.read(std::span{in.target_joints});

	in.name.resize(static_cast<std::size_t>(header.name_length));
	file.read(std::span{in.name.data(), in.name.size()});

	if (in.compute_hash() != header.hash) { return false; }

	*this = std::move(in);
	return true;
}
} // namespace asset

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

void asset::from_json(dj::Json const& json, Skeleton& out) {
	assert(json["asset_type"].as_string() == "skeleton");
	out.name = json["name"].as_string();
	for (auto const& in_joint : json["joints"].array_view()) {
		auto& out_joint = out.joints.emplace_back();
		out_joint.name = in_joint["name"].as_string();
		from_json(in_joint["transform"], out_joint.transform);
		out_joint.self = in_joint["self"].as<std::size_t>();
		if (auto const& in_parent = in_joint["parent"]) { out_joint.parent = in_parent.as<std::size_t>(); }
		for (auto const& in_child : in_joint["children"].array_view()) { out_joint.children.push_back(in_child.as<std::size_t>()); }
	}
	for (auto const& in_animation : json["animations"].array_view()) { out.animations.push_back(std::string{in_animation.as_string()}); }
}

void asset::to_json(dj::Json& out, Skeleton const& asset) {
	out["asset_type"] = "skeleton";
	out["name"] = asset.name;
	auto out_joints = dj::Json{};
	for (auto const& in_joint : asset.joints) {
		auto out_joint = dj::Json{};
		out_joint["name"] = in_joint.name;
		to_json(out_joint["transform"], in_joint.transform);
		out_joint["self"] = in_joint.self;
		if (in_joint.parent) { out_joint["parent"] = *in_joint.parent; }
		if (!in_joint.children.empty()) {
			auto out_children = dj::Json{};
			for (auto const child : in_joint.children) { out_children.push_back(child); }
			out_joint["children"] = std::move(out_children);
		}
		out_joints.push_back(std::move(out_joint));
	}
	out["joints"] = std::move(out_joints);
	if (!asset.animations.empty()) {
		auto out_animations = dj::Json{};
		for (auto const& in_animation : asset.animations) { out_animations.push_back(in_animation.value()); }
		out["animations"] = std::move(out_animations);
	}
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
} // namespace levk
