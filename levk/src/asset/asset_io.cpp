#include <fmt/format.h>
#include <glm/gtx/matrix_decompose.hpp>
#include <levk/asset/asset_io.hpp>
#include <levk/io/common.hpp>
#include <levk/node/node_tree_serializer.hpp>
#include <levk/util/binary_file.hpp>
#include <levk/util/hash_combine.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/visitor.hpp>
#include <charconv>
#include <sstream>

namespace levk {
namespace {
constexpr auto type_names_v{EnumArray<asset::Type, std::string_view>{
	"Unknown",
	"Quad",
	"Cube",
	"Sphere",
	"ViewPlane",
	"Camera",
	"Lights",
	"NodeTree",
	"Material",
	"Skeleton",
	"Mesh",
	"Level",
}};

static_assert(std::size(type_names_v.t) == std::size_t(asset::Type::eCOUNT_));

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
	static auto const s_log{Logger{"BinSkeletonAnimation"}};

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
		default: s_log.error("Unrecognized type: [{}]", static_cast<int>(sampler_header.type)); break;
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

bool check_asset_type(asset::Type const type, dj::Json const& json) {
	if (type == asset::Type::eUnknown) { return false; }
	auto in_type = asset::Type{};
	from_json(json, in_type);
	return in_type == type;
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

constexpr std::string_view asset_type_key_v{"asset_type"};
} // namespace asset

auto asset::get_type(std::string_view const str) -> Type {
	for (Type type{}; type < Type::eCOUNT_; type = Type(int(type) + 1)) {
		if (type_names_v[type] == str) { return type; }
	}
	return Type::eUnknown;
}

void asset::from_json(dj::Json const& json, Type& out) { out = get_type(json[asset_type_key_v].as_string()); }

void asset::to_json(dj::Json& out, Type const& type) { out[asset_type_key_v] = type_names_v[type]; }

void asset::from_json(dj::Json const& json, Quad& out) {
	assert(check_asset_type(asset::Type::eQuad, json));
	levk::from_json(json["size"], out.size, out.size);
	from_json(json["uv_rect"], out.uv, out.uv);
	from_json(json["rgb"], out.rgb);
	levk::from_json(json["origin"], out.origin, out.origin);
}

void asset::to_json(dj::Json& out, Quad const& quad) {
	to_json(out, asset::Type::eQuad);
	levk::to_json(out["size"], quad.size);
	to_json(out["uv_rect"], quad.uv);
	to_json(out["rgb"], quad.rgb);
	levk::to_json(out["origin"], quad.origin);
}

void asset::from_json(dj::Json const& json, Cube& out) {
	assert(check_asset_type(asset::Type::eCube, json));
	levk::from_json(json["size"], out.size, out.size);
	from_json(json["rgb"], out.rgb);
	levk::from_json(json["origin"], out.origin, out.origin);
}

void asset::to_json(dj::Json& out, Cube const& cube) {
	to_json(out, asset::Type::eCube);
	levk::to_json(out["size"], cube.size);
	to_json(out["rgb"], cube.rgb);
	levk::to_json(out["origin"], cube.origin);
}

void asset::from_json(dj::Json const& json, Sphere& out) {
	assert(check_asset_type(asset::Type::eSphere, json));
	out.diameter = json["diameter"].as<float>(out.diameter);
	out.resolution = json["resolution"].as<std::uint32_t>(out.resolution);
	from_json(json["rgb"], out.rgb);
	levk::from_json(json["origin"], out.origin);
}

void asset::to_json(dj::Json& out, Sphere const& sphere) {
	to_json(out, asset::Type::eSphere);
	out["size"] = sphere.diameter;
	out["resolution"] = sphere.resolution;
	to_json(out["rgb"], sphere.rgb);
	levk::to_json(out["origin"], sphere.origin);
}

void asset::from_json(dj::Json const& json, ViewPlane& out) {
	assert(check_asset_type(asset::Type::eViewPlane, json));
	out.near = json["near"].as<float>(out.near);
	out.far = json["far"].as<float>(out.far);
}

void asset::to_json(dj::Json& out, ViewPlane const& view_plane) {
	to_json(out, asset::Type::eViewPlane);
	out["near"] = view_plane.near;
	out["far"] = view_plane.far;
}

void asset::from_json(dj::Json const& json, Camera& out) {
	assert(check_asset_type(asset::Type::eCamera, json));
	out.name = json["name"].as<std::string>();
	levk::from_json(json["transform"], out.transform);
	out.transform.recompute();
	out.exposure = json["exposure"].as<float>(out.exposure);
	if (json["type"].as_string() == "orthographic") {
		auto type = Camera::Orthographic{};
		from_json(json["view_plane"], type.view_plane);
		out.type = type;
	} else {
		auto type = Camera::Perspective{};
		from_json(json["view_plane"], type.view_plane);
		auto const in_degrees = json["field_of_view"].as<float>(Degrees{type.field_of_view}.value);
		type.field_of_view = Degrees{in_degrees};
		out.type = type;
	}
}

void asset::to_json(dj::Json& out, Camera const& camera) {
	to_json(out, asset::Type::eCamera);
	out["name"] = camera.name;
	levk::to_json(out["transform"], camera.transform);
	out["exposure"] = camera.exposure;
	auto const camera_visitor = Visitor{
		[&out](Camera::Perspective const& perspective) {
			out["type"] = "perspective";
			out["field_of_view"] = perspective.field_of_view.to_degrees().value;
			to_json(out["view_plane"], perspective.view_plane);
		},
		[&out](Camera::Orthographic const& orthographic) {
			out["type"] = "orthographic";
			to_json(out["view_plane"], orthographic.view_plane);
		},
	};
	std::visit(camera_visitor, camera.type);
}

void asset::from_json(dj::Json const& json, Lights& out) {
	assert(check_asset_type(asset::Type::eLights, json));
	if (auto const& in_primary_light = json["primary"]) {
		levk::from_json(in_primary_light["direction"], out.primary.direction);
		from_json(in_primary_light["rgb"], out.primary.rgb);
	}
	auto const& in_dir_lights = json["dir_lights"];
	if (!in_dir_lights.array_view().empty()) { out.dir_lights.clear(); }
	for (auto const& in_dir_light : in_dir_lights.array_view()) {
		auto& out_dir_light = out.dir_lights.emplace_back();
		levk::from_json(in_dir_light["direction"], out_dir_light.direction);
		from_json(in_dir_light["rgb"], out_dir_light.rgb);
	}
}

void asset::to_json(dj::Json& out, Lights const& lights) {
	to_json(out, asset::Type::eLights);
	auto& out_primary_light = out["primary"];
	levk::to_json(out_primary_light["direction"], lights.primary.direction);
	to_json(out_primary_light["rgb"], lights.primary.rgb);
	if (!lights.dir_lights.empty()) {
		auto& out_dir_lights = out["dir_lights"];
		for (auto const& in_dir_light : lights.dir_lights) {
			auto& out_dir_light = out_dir_lights.push_back({});
			levk::to_json(out_dir_light["direction"], in_dir_light.direction);
			levk::to_json(out_dir_light["rgb"], in_dir_light.rgb);
		}
	}
}

void asset::from_json(dj::Json const& json, NodeTree& out) {
	assert(check_asset_type(asset::Type::eNodeTree, json));
	NodeTree::Serializer::deserialize(json, out);
}

void asset::to_json(dj::Json& out, NodeTree const& node_tree) {
	to_json(out, asset::Type::eNodeTree);
	NodeTree::Serializer::serialize(out, node_tree);
}

void asset::from_json(dj::Json const& json, Level& out) {
	assert(check_asset_type(asset::Type::eLevel, json));
	out.name = json["name"].as_string(out.name);
	from_json(json["node_tree"], out.node_tree);
	from_json(json["camera"], out.camera);
	from_json(json["lights"], out.lights);
	auto add_instances = [](dj::Json const& json, std::vector<Transform>& out_instances) {
		for (auto const& instance : json.array_view()) { levk::from_json(instance, out_instances.emplace_back()); }
	};
	for (auto const& [node_id_str, in_attachment] : json["attachments"].object_view()) {
		auto const get_node_id = [](std::string_view const in) -> Id<Node>::id_type {
			auto ret = Id<Node>::id_type{};
			auto [_, ec] = std::from_chars(in.data(), in.data() + in.size(), ret);
			if (ec != std::errc{}) { return {}; }
			return ret;
		};
		auto const node_id = get_node_id(node_id_str);
		if (!node_id) { continue; }
		auto& out_attachment = out.attachments[node_id];
		out_attachment.mesh_uri = in_attachment["mesh_uri"].as_string();
		out_attachment.shape = in_attachment["shape"];
		if (auto const& enabled_animation = in_attachment["enabled_animation"]) { out_attachment.enabled_animation = enabled_animation.as<std::size_t>(); }
		add_instances(in_attachment["mesh_instances"], out_attachment.mesh_instances);
		add_instances(in_attachment["shape_instances"], out_attachment.shape_instances);
	}
}

void asset::to_json(dj::Json& out, Level const& level) {
	to_json(out, asset::Type::eLevel);
	out["name"] = level.name;
	to_json(out["node_tree"], level.node_tree);
	to_json(out["camera"], level.camera);
	to_json(out["lights"], level.lights);
	if (!level.attachments.empty()) {
		auto add_instances = [](dj::Json& out_json, std::span<Transform const> instances) {
			for (auto const& transform : instances) { levk::to_json(out_json.push_back({}), transform); }
		};
		auto& out_attachments = out["attachments"];
		for (auto const& [node_id, attachment] : level.attachments) {
			auto& out_attachment = out_attachments[std::to_string(node_id)];
			if (attachment.mesh_uri) { out_attachment["mesh_uri"] = attachment.mesh_uri.value(); }
			if (attachment.shape) { out_attachment["shape"] = attachment.shape; }
			if (attachment.enabled_animation) { out_attachment["enabled_animation"] = *attachment.enabled_animation; }
			if (!attachment.mesh_instances.empty()) { add_instances(out["mesh_instances"], attachment.mesh_instances); }
			if (!attachment.shape_instances.empty()) { add_instances(out["shape_instances"], attachment.shape_instances); }
		}
	}
}

void asset::from_json(dj::Json const& json, asset::Material& out) {
	assert(check_asset_type(asset::Type::eMaterial, json));
	out.textures.deserialize(json["textures"]);
	levk::from_json(json["albedo"], out.albedo);
	levk::from_json(json["emissive_factor"], out.emissive_factor);
	out.metallic = json["metallic"].as<float>(out.metallic);
	out.roughness = json["roughness"].as<float>(out.roughness);
	levk::from_json(json["render_mode"], out.render_mode);
	out.alpha_cutoff = json["alpha_cutoff"].as<float>(out.alpha_cutoff);
	out.alpha_mode = to_alpha_mode(json["alpha_mode"].as_string());
	out.vertex_shader = json["vertex_shader"].as_string(out.vertex_shader);
	out.fragment_shader = json["fragment_shader"].as_string(out.fragment_shader);
	out.name = json["name"].as_string();
}

void asset::to_json(dj::Json& out, asset::Material const& asset) {
	to_json(out, asset::Type::eMaterial);
	out["type_name"] = asset.vertex_shader.find("skinned") != std::string_view::npos ? SkinnedMaterial{}.type_name() : LitMaterial{}.type_name();
	asset.textures.serialize(out["textures"]);
	levk::to_json(out["albedo"], asset.albedo);
	levk::to_json(out["emissive_factor"], asset.emissive_factor);
	out["metallic"] = asset.metallic;
	out["roughness"] = asset.roughness;
	levk::to_json(out["render_mode"], asset.render_mode);
	out["alpha_cutoff"] = asset.alpha_cutoff;
	out["alpha_mode"] = from(asset.alpha_mode);
	out["vertex_shader"] = asset.vertex_shader;
	out["fragment_shader"] = asset.fragment_shader;
	out["name"] = asset.name;
}

void asset::from_json(dj::Json const& json, Skeleton& out) {
	assert(check_asset_type(asset::Type::eSkeleton, json));
	out.name = json["name"].as_string();
	from_json(json["joint_tree"], out.joint_tree);
	auto const& in_ordered_joints = json["ordered_joints"].array_view();
	out.ordered_joints_ids.reserve(in_ordered_joints.size());
	for (auto const& in_joint : in_ordered_joints) { out.ordered_joints_ids.push_back(in_joint.as<Id<Node>::id_type>()); }
	for (auto const& in_animation : json["animations"].array_view()) { out.animations.push_back(std::string{in_animation.as_string()}); }
}

void asset::to_json(dj::Json& out, Skeleton const& asset) {
	to_json(out, asset::Type::eSkeleton);
	out["name"] = asset.name;
	to_json(out["joint_tree"], asset.joint_tree);
	auto& out_joints = out["ordered_joints"];
	for (auto const joint : asset.ordered_joints_ids) { out_joints.push_back(joint.value()); }
	if (!asset.animations.empty()) {
		auto out_animations = dj::Json{};
		for (auto const& in_animation : asset.animations) { out_animations.push_back(in_animation.value()); }
		out["animations"] = std::move(out_animations);
	}
}

void asset::from_json(dj::Json const& json, Mesh3D& out) {
	assert(check_asset_type(asset::Type::eMesh, json));
	out.skeleton = std::string{json["skeleton"].as_string()};
	out.type = out.skeleton.is_empty() ? Mesh3D::Type::eStatic : Mesh3D::Type::eSkinned;
	for (auto const& in_primitive : json["primitives"].array_view()) {
		auto primitive = Mesh3D::Primitive{};
		primitive.geometry = std::string{in_primitive["geometry"].as_string()};
		primitive.material = std::string{in_primitive["material"].as_string()};
		out.primitives.push_back(std::move(primitive));
	}
	for (auto const& in_ibm : json["inverse_bind_matrices"].array_view()) { levk::from_json(in_ibm, out.inverse_bind_matrices.emplace_back()); }
	out.name = json["name"].as_string();
}

void asset::to_json(dj::Json& out, Mesh3D const& asset) {
	to_json(out, asset::Type::eMesh);
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
		for (auto const& in_ibm : asset.inverse_bind_matrices) { levk::to_json(ibm.push_back({}), in_ibm); }
	}
	out["name"] = asset.name;
}
} // namespace levk
