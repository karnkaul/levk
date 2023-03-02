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

HdrRgba to_hdr_rgba(std::string_view hex, float intensity) {
	assert(!hex.empty());
	if (hex[0] == '#') { hex = hex.substr(1); }
	assert(hex.size() == 8);
	auto ret = HdrRgba{};
	ret.channels.x = to_channel(hex.substr(0, 2));
	ret.channels.y = to_channel(hex.substr(2, 2));
	ret.channels.z = to_channel(hex.substr(4, 2));
	ret.channels.w = to_channel(hex.substr(6));
	ret.intensity = intensity;
	return ret;
}

template <glm::length_t Dim>
void add_to(dj::Json& out, glm::vec<Dim, float> const& vec) {
	out.push_back(vec.x);
	if constexpr (Dim > 1) { out.push_back(vec.y); }
	if constexpr (Dim > 2) { out.push_back(vec.z); }
	if constexpr (Dim > 3) { out.push_back(vec.w); }
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

template <typename T, typename SourceT>
T get_sampler(asset::BinSkeletalAnimation::Header::Sampler const& header, BinaryIn<SourceT>& file) {
	auto ret = T{};
	ret.interpolation = header.interpolation;
	ret.keyframes.resize(static_cast<std::size_t>(header.keyframes));
	file.read(std::span{ret.keyframes});
	return ret;
}

template <typename BinarySourceT>
bool read_from(BinarySourceT&& source, asset::BinGeometry& out) {
	auto bin = BinaryIn<BinarySourceT>{std::move(source)};
	if (!bin) { return false; }

	auto in = asset::BinGeometry{};
	auto header = asset::BinGeometry::Header{};
	bin.read(header);

	in.geometry.positions.resize(static_cast<std::size_t>(header.positions));
	bin.read(std::span{in.geometry.positions});

	in.geometry.rgbs.resize(static_cast<std::size_t>(header.positions));
	bin.read(std::span{in.geometry.rgbs});

	in.geometry.normals.resize(static_cast<std::size_t>(header.positions));
	bin.read(std::span{in.geometry.normals});

	in.geometry.uvs.resize(static_cast<std::size_t>(header.positions));
	bin.read(std::span{in.geometry.uvs});

	if (header.indices) {
		in.geometry.indices.resize(static_cast<std::size_t>(header.indices));
		bin.read(std::span{in.geometry.indices});
	}

	if (header.joints) {
		in.joints.resize(static_cast<std::size_t>(header.joints));
		bin.read(std::span{in.joints});

		assert(header.weights == header.joints);
		in.weights.resize(static_cast<std::size_t>(header.weights));
		bin.read(std::span{in.weights});
	}

	if (in.compute_hash() != header.hash) { return false; }

	out = std::move(in);
	return true;
}

template <typename BinarySourceT>
bool read_from(BinarySourceT&& source, asset::BinSkeletalAnimation& out) {
	auto bin = BinaryIn<BinarySourceT>{std::move(source)};
	if (!bin) { return false; }

	auto in = asset::BinSkeletalAnimation{};
	auto header = asset::BinSkeletalAnimation::Header{};
	bin.read(header);

	for (std::uint64_t i = 0; i < header.samplers; ++i) {
		auto sampler_header = asset::BinSkeletalAnimation::Header::Sampler{};
		bin.read(sampler_header);
		auto& out_sampler = in.samplers.emplace_back();
		switch (sampler_header.type) {
		case asset::BinSkeletalAnimation::Type::eTranslate: out_sampler = get_sampler<TransformAnimation::Translate>(sampler_header, bin); break;
		case asset::BinSkeletalAnimation::Type::eRotate: out_sampler = get_sampler<TransformAnimation::Rotate>(sampler_header, bin); break;
		case asset::BinSkeletalAnimation::Type::eScale: out_sampler = get_sampler<TransformAnimation::Scale>(sampler_header, bin); break;
		default: logger::error("[BinSkeletonAnimation] Unrecognized type: [{}]", static_cast<int>(sampler_header.type)); break;
		}
	}

	in.target_joints.resize(static_cast<std::size_t>(header.target_joints));
	bin.read(std::span{in.target_joints});

	in.name.resize(static_cast<std::size_t>(header.name_length));
	bin.read(std::span{in.name.data(), in.name.size()});

	if (in.compute_hash() != header.hash) { return false; }

	out = std::move(in);
	return true;
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

bool BinGeometry::read(char const* path) { return read_from(BinarySourceFile{path}, *this); }
bool BinGeometry::read(std::span<std::byte const> bytes) { return read_from(BinarySourceData{bytes}, *this); }

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

bool BinSkeletalAnimation::read(char const* path) { return read_from(BinarySourceFile{path}, *this); }
bool BinSkeletalAnimation::read(std::span<std::byte const> bytes) { return read_from(BinarySourceData{bytes}, *this); }
} // namespace asset

void asset::from_json(dj::Json const& json, glm::quat& out, glm::quat const& fallback) {
	auto vec = glm::vec4{};
	from_json(json, vec, {fallback.x, fallback.y, fallback.z, fallback.w});
	out = glm::quat{vec.w, vec.x, vec.y, vec.z};
}

void asset::to_json(dj::Json& out, glm::quat const& quat) {
	auto const vec = glm::vec4{quat.x, quat.y, quat.z, quat.w};
	to_json(out, vec);
}

void asset::from_json(dj::Json const& json, glm::mat4& out) {
	out[0] = glm_vec_from_json<4>(json, out[0]);
	out[1] = glm_vec_from_json<4>(json, out[1], 4);
	out[2] = glm_vec_from_json<4>(json, out[2], 8);
	out[3] = glm_vec_from_json<4>(json, out[3], 12);
}

void asset::to_json(dj::Json& out, glm::mat4 const& mat) {
	for (glm::length_t i = 0; i < 4; ++i) { add_to(out, mat[i]); }
}

void asset::to_json(dj::Json& out, Rgba const& rgba) {
	auto hdr = HdrRgba{rgba, 1.0f};
	return to_json(out, hdr);
}

void asset::from_json(dj::Json const& json, Rgba& out) {
	auto hdr = HdrRgba{};
	from_json(json, hdr);
	out = hdr;
}

void asset::to_json(dj::Json& out, HdrRgba const& rgba) {
	out["hex"] = to_hex(rgba);
	out["intensity"] = rgba.intensity;
}

void asset::from_json(dj::Json const& json, HdrRgba& out) { out = to_hdr_rgba(json["hex"].as_string(), json["intensity"].as<float>(1.0f)); }

void asset::from_json(dj::Json const& json, Transform& out) {
	auto mat = glm::mat4{1.0f};
	from_json(json, mat);
	out.decompose(mat);
}

void asset::to_json(dj::Json& out, Transform const& transform) { to_json(out, transform.matrix()); }

void asset::from_json(dj::Json const& json, RenderMode& out) {
	out.line_width = json["line_width"].as<float>(out.line_width);
	out.type = to_render_mode_type(json["type"].as_string());
	out.depth_test = json["depth_test"].as<bool>();
}

void asset::to_json(dj::Json& out, RenderMode const& render_mode) {
	out["line_width"] = render_mode.line_width;
	out["type"] = from(render_mode.type);
	out["depth_test"] = dj::Boolean{render_mode.depth_test};
}

void asset::from_json(dj::Json const& json, asset::Material& out) {
	assert(json["asset_type"].as_string() == "material");
	out.textures.deserialize(json["textures"]);
	from_json(json["albedo"], out.albedo);
	out.emissive_factor = glm_vec_from_json<3>(json["emissive_factor"], out.emissive_factor);
	out.metallic = json["metallic"].as<float>(out.metallic);
	out.roughness = json["roughness"].as<float>(out.roughness);
	from_json(json["render_mode"], out.render_mode);
	out.alpha_cutoff = json["alpha_cutoff"].as<float>(out.alpha_cutoff);
	out.alpha_mode = to_alpha_mode(json["alpha_mode"].as_string());
	out.shader = json["shader"].as_string(out.shader);
	out.name = json["name"].as_string();
}

void asset::to_json(dj::Json& out, asset::Material const& asset) {
	static auto const s_type_name = LitMaterial{}.type_name();
	out["type_name"] = s_type_name;
	out["asset_type"] = "material";
	asset.textures.serialize(out["textures"]);
	to_json(out["albedo"], asset.albedo);
	to_json(out["emissive_factor"], asset.emissive_factor);
	out["metallic"] = asset.metallic;
	out["roughness"] = asset.roughness;
	to_json(out["render_mode"], asset.render_mode);
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
	for (auto const& in_ibm : json["inverse_bind_matrices"].array_view()) { from_json(in_ibm, out.inverse_bind_matrices.emplace_back()); }
	out.name = json["name"].as_string();
}

void asset::to_json(dj::Json& out, Mesh const& asset) {
	out["asset_type"] = "mesh";
	out["type"] = asset.type == Mesh::Type::eSkinned ? "skinned" : "static";
	if (!asset.primitives.empty()) {
		auto& primitives = out["primitives"];
		for (auto const& in_primitive : asset.primitives) {
			auto& out_primitive = primitives.push_back({});
			out_primitive["geometry"] = in_primitive.geometry.value();
			if (!in_primitive.material.value().empty()) { out_primitive["material"] = in_primitive.material.value(); }
		}
	}
	if (!asset.skeleton.value().empty()) {
		out["skeleton"] = asset.skeleton.value();
		assert(!asset.inverse_bind_matrices.empty());
		auto& ibm = out["inverse_bind_matrices"];
		for (auto const& in_ibm : asset.inverse_bind_matrices) { to_json(ibm.push_back({}), in_ibm); }
	}
	out["name"] = asset.name;
}
} // namespace levk
