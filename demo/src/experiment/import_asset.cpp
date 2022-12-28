#include <experiment/import_asset.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <gltf2cpp/gltf2cpp.hpp>
#include <levk/util/binary_file.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/error.hpp>
#include <levk/util/hash_combine.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/visitor.hpp>
#include <levk/util/zip_ranges.hpp>
#include <filesystem>
#include <fstream>

namespace levk {
namespace fs = std::filesystem;

namespace experiment {
namespace {
constexpr AlphaMode from(gltf2cpp::AlphaMode const mode) {
	switch (mode) {
	default:
	case gltf2cpp::AlphaMode::eOpaque: return AlphaMode::eOpaque;
	case gltf2cpp::AlphaMode::eBlend: return AlphaMode::eBlend;
	case gltf2cpp::AlphaMode::eMask: return AlphaMode::eMask;
	}
}

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

glm::mat4x4 from(gltf2cpp::Mat4x4 const& in) {
	auto ret = glm::mat4x4{};
	std::memcpy(&ret[0], &in[0], sizeof(ret[0]));
	std::memcpy(&ret[1], &in[1], sizeof(ret[1]));
	std::memcpy(&ret[2], &in[2], sizeof(ret[2]));
	std::memcpy(&ret[3], &in[3], sizeof(ret[3]));
	return ret;
}

Transform from(glm::mat4 const& mat) {
	auto ret = Transform{};
	glm::vec3 scale{1.0f}, pos{}, skew{};
	glm::vec4 persp{};
	glm::quat orn{quat_identity_v};
	glm::decompose(mat, scale, orn, pos, skew, persp);
	ret.set_position(pos);
	ret.set_orientation(orn);
	ret.set_scale(scale);
	return ret;
}

Transform from(gltf2cpp::Transform const& transform) {
	auto ret = Transform{};
	auto visitor = Visitor{
		[&ret](gltf2cpp::Trs const& trs) {
			ret.set_position({trs.translation[0], trs.translation[1], trs.translation[2]});
			ret.set_orientation({trs.rotation[3], trs.rotation[0], trs.rotation[1], trs.rotation[2]});
			ret.set_scale({trs.scale[0], trs.scale[1], trs.scale[2]});
		},
		[&ret](gltf2cpp::Mat4x4 const& mat) { ret = from(from(mat)); },
	};
	std::visit(visitor, transform);
	return ret;
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

template <glm::length_t Dim>
std::vector<glm::vec<Dim, float>> from(std::vector<gltf2cpp::Vec<Dim>> const in) {
	if (in.empty()) { return {}; }
	auto ret = std::vector<glm::vec<Dim, float>>(in.size());
	std::memcpy(ret.data(), in.data(), std::span<gltf2cpp::Vec<Dim> const>{in}.size_bytes());
	return ret;
}

template <typename T>
Interpolator<T> make_interpolator(std::span<float const> times, std::span<T const> values, gltf2cpp::Interpolation interpolation) {
	assert(times.size() == values.size());
	auto ret = Interpolator<T>{};
	for (auto [t, v] : zip_ranges(times, values)) { ret.keyframes.push_back({v, t}); }
	ret.interpolation = static_cast<Interpolation>(interpolation);
	return ret;
}

Geometry::Packed to_geometry(gltf2cpp::Mesh::Primitive const& primitive) {
	auto ret = Geometry::Packed{};
	ret.positions = from<3>(primitive.geometry.positions);
	if (!primitive.geometry.colors.empty()) { ret.rgbs = from<3>(primitive.geometry.colors[0]); }
	if (ret.rgbs.empty()) { ret.rgbs = std::vector<glm::vec3>(ret.positions.size(), glm::vec3{1.0f}); }
	ret.normals = from<3>(primitive.geometry.normals);
	if (ret.normals.empty()) { ret.normals = std::vector<glm::vec3>(ret.positions.size(), glm::vec3{0.0f, 0.0f, 1.0f}); }
	if (!primitive.geometry.tex_coords.empty()) { ret.uvs = from<2>(primitive.geometry.tex_coords[0]); }
	if (ret.uvs.empty()) { ret.uvs = std::vector<glm::vec2>(ret.positions.size()); }
	ret.indices = primitive.geometry.indices;
	return ret;
}

template <typename T>
using Index = Skeleton::Index<T>;

// Joints in a GLTF skin reference nodes in the GLTF global tree via their indices.
// To construct a skinned mesh skeleton and its animations, the tree needs to be trimmed to
// only the nodes relevant to the skin, and node indices correspondingly retargeted.
// A skeleton instance will then add one node per joint to the engine's scene tree, copy
// the animations and rewrite each animator target to match its corresponding Id<Node>.
// The order of joints after all these transformations must be identical to the source data,
// hence the [Index => *] unordered_map invasion.
struct MapGltfNodesToJoints {
	struct Entry {
		Skeleton::Joint joint{};
		Index<Skeleton::Joint> index{};
	};

	struct Result {
		std::vector<Skeleton::Joint> joints{};
		std::unordered_map<Index<gltf2cpp::Node>, Index<Skeleton::Joint>> map{};
	};

	std::unordered_map<Index<gltf2cpp::Node>, Entry> map{};
	std::vector<Skeleton::Joint> joints{};

	std::span<gltf2cpp::Node const> nodes{};

	void add_node_and_children(Index<Node> node_idx, std::optional<Index<Skeleton::Joint>> parent = {}) {
		if (map.contains(node_idx)) { return; }
		auto const& node = nodes[node_idx];
		auto entry = Entry{};
		entry.joint.name = node.name;
		entry.joint.transform = from(node.transform);
		entry.joint.children = {node.children.begin(), node.children.end()};
		entry.joint.parent = parent;
		entry.index = joints.size();
		map.insert_or_assign(node_idx, std::move(entry));
		for (auto const child : node.children) { add_node_and_children(child, entry.index); }
	}

	[[nodiscard]] Result operator()(gltf2cpp::Skin const& skin, std::span<gltf2cpp::Node const> nodes) {
		auto ret = Result{};
		this->nodes = nodes;
		for (auto const& node_idx : skin.joints) { add_node_and_children(node_idx); }
		auto old = std::move(joints);
		for (auto const node_idx : skin.joints) {
			assert(map.contains(node_idx));
			auto& entry = map[node_idx];
			entry.joint.self = entry.index = joints.size();
			joints.push_back(std::move(entry.joint));
		}
		for (auto& joint : joints) {
			for (auto& child : joint.children) {
				assert(map.contains(child));
				child = map[child].index;
			}
		}
		ret.joints = std::move(joints);
		for (auto const& [i, e] : map) { ret.map.insert_or_assign(i, e.index); }
		return ret;
	}
};

struct ExportMeshes {
	std::string_view json_name;
	gltf2cpp::Root& in_root;
	fs::path in_path;
	fs::path out_path;

	ImportedMeshes result{};

	struct {
		std::unordered_map<Index<gltf2cpp::Image>, std::string> images{};
		std::unordered_map<Index<gltf2cpp::Geometry>, std::string> geometries{};
		std::unordered_map<Index<gltf2cpp::Material>, std::string> materials{};
		std::unordered_map<Index<gltf2cpp::Mesh>, std::string> meshes{};
		std::unordered_map<Index<gltf2cpp::Skin>, std::string> skeletons{};
	} exported{};

	fs::path unique_filename(fs::path in, std::string_view extension, std::string_view fallback) {
		int duplicate_filename = 1;
		auto out = in.string();
		if (out.empty() || out == "(Unnamed)") {
			assert(!fallback.empty());
			in = fallback;
			out = in;
		}
		auto path = out_path / fmt::format("{}{}", out, extension);
		static constexpr auto max_iter_v = 1000;
		for (; fs::exists(path) && duplicate_filename < max_iter_v; ++duplicate_filename) {
			out = fmt::format("{}_{}", in.string(), duplicate_filename);
			path = out_path / fmt::format("{}{}", out, extension);
		}
		assert(duplicate_filename < max_iter_v && !fs::exists(path));
		return fmt::format("{}{}", out, extension);
	}

	std::string copy_image(gltf2cpp::Image const& in, std::size_t index) {
		assert(!in.source_filename.empty());
		if (auto it = exported.images.find(index); it != exported.images.end()) { return it->second; }
		auto filename = fs::path{in.source_filename};
		auto extension = std::string{};
		auto uri = std::string{};
		if (filename.empty()) {
			uri = unique_filename({}, ".bin", "image");
		} else {
			auto extension = filename.extension().string();
			filename = filename.stem();
			uri = unique_filename(filename, extension, {});
		}
		auto dst = out_path / uri;
		fs::create_directories(dst.parent_path());
		fs::copy_file(in_path / in.source_filename, out_path / uri);
		logger::info("[Import] Image [{}] copied from [{}]", uri, in.source_filename);
		exported.images.insert_or_assign(index, uri);
		return uri;
	}

	std::string copy_image(gltf2cpp::Texture const& in, std::size_t index) { return copy_image(in_root.images[in.source], index); }

	AssetUri<Material> export_material(gltf2cpp::Material const& in, std::size_t index) {
		if (auto it = exported.materials.find(index); it != exported.materials.end()) { return it->second; }
		auto material = AssetMaterial{};
		material.albedo = Rgba::from(glm::vec4{in.pbr.base_color_factor[0], in.pbr.base_color_factor[1], in.pbr.base_color_factor[2], 1.0f});
		material.metallic = in.pbr.metallic_factor;
		material.roughness = in.pbr.roughness_factor;
		material.alpha_mode = from(in.alpha_mode);
		material.alpha_cutoff = in.alpha_cutoff;
		material.name = in.name;
		if (auto i = in.pbr.base_color_texture) { material.base_colour = copy_image(in_root.textures[i->texture], i->texture); }
		if (auto i = in.pbr.metallic_roughness_texture) { material.roughness_metallic = copy_image(in_root.textures[i->texture], i->texture); }
		if (auto i = in.emissive_texture) { material.emissive = copy_image(in_root.textures[i->texture], i->texture); }
		material.emissive_factor = {in.emissive_factor[0], in.emissive_factor[1], in.emissive_factor[2]};
		auto uri = unique_filename(in.name, ".json", "material");
		auto json = dj::Json{};
		to_json(json, material);
		json.to_file((out_path / uri).string().c_str());
		auto ret = uri.generic_string();
		logger::info("[Import] Material [{}] imported", ret);
		exported.materials.insert_or_assign(index, ret);
		return ret;
	}

	AssetUri<BinGeometry> export_geometry(gltf2cpp::Mesh::Primitive const& in, std::size_t index, std::vector<glm::uvec4> joints,
										  std::vector<glm::vec4> weights) {
		if (auto it = exported.geometries.find(index); it != exported.geometries.end()) { return it->second; }
		auto bin = BinGeometry{};
		bin.geometry = to_geometry(in);
		bin.joints = std::move(joints);
		bin.weights = std::move(weights);
		auto filename = fmt::format("geometry{}", index);
		auto uri = unique_filename(filename, ".bin", {});
		[[maybe_unused]] bool const res = bin.write((out_path / uri).string().c_str());
		assert(res);
		auto ret = uri.generic_string();
		logger::info("[Import] BinGeometry [{}] imported", ret);
		exported.geometries.insert_or_assign(index, ret);
		return ret;
	}

	AssetUri<SkinnedMesh> export_mesh(gltf2cpp::Mesh const& in_mesh, std::size_t index, AssetUri<Skeleton> skeleton) {
		if (auto it = exported.meshes.find(index); it != exported.meshes.end()) { return it->second; }
		auto out_mesh = AssetMesh{};
		out_mesh.type = skeleton.value().empty() ? AssetMesh::Type::eStatic : AssetMesh::Type::eSkinned;
		for (auto const& [in_primitive, primitive_index] : enumerate(in_mesh.primitives)) {
			auto out_primitive = AssetMesh::Primitive{};
			if (in_primitive.material) { out_primitive.material = export_material(in_root.materials[*in_primitive.material], *in_primitive.material); }
			assert(!(in_primitive.geometry.joints.empty() ^ skeleton.value().empty()));
			auto joints = std::vector<glm::uvec4>{};
			auto weights = std::vector<glm::vec4>{};
			if (!in_primitive.geometry.joints.empty()) {
				joints.resize(in_primitive.geometry.joints[0].size());
				std::memcpy(joints.data(), in_primitive.geometry.joints[0].data(), std::span{in_primitive.geometry.joints[0]}.size_bytes());
				weights.resize(in_primitive.geometry.weights[0].size());
				std::memcpy(weights.data(), in_primitive.geometry.weights[0].data(), std::span{in_primitive.geometry.weights[0]}.size_bytes());
			}
			out_primitive.geometry = export_geometry(in_primitive, primitive_index, std::move(joints), std::move(weights));
			out_mesh.primitives.push_back(std::move(out_primitive));
		}
		if (out_mesh.primitives.empty()) {
			logger::warn("[Import] Mesh has no primitives: [{}] (index {}), skipping", in_mesh.name, index);
			return {};
		}
		if (!skeleton.value().empty()) { out_mesh.skeleton = std::move(skeleton); }
		auto uri = unique_filename(out_mesh.name, ".json", json_name);
		out_mesh.name = fs::path{uri}.stem().string();
		auto json = dj::Json{};
		to_json(json, out_mesh);
		json.to_file((out_path / uri).string().c_str());
		auto ret = uri.generic_string();
		logger::info("[Import] Mesh [{}] imported", ret);
		exported.meshes.insert_or_assign(index, ret);
		result.meshes.push_back(uri.generic_string());
		return ret;
	}

	AssetUri<Skeleton> export_skeleton(gltf2cpp::Skin const& in, std::size_t index) {
		auto skin_node = Ptr<gltf2cpp::Node const>{};
		for (auto& node : in_root.nodes) {
			if (node.skin && *node.skin == index) { skin_node = &node; }
		}
		if (!skin_node) { return {}; }

		assert(skin_node->mesh.has_value());
		if (auto it = exported.skeletons.find(index); it != exported.skeletons.end()) { return it->second; }

		auto [joints, map] = MapGltfNodesToJoints{}(in, in_root.nodes);
		auto skeleton = Skeleton{.joints = std::move(joints)};
		for (auto const& in_animation : in_root.animations) {
			auto clip = Skeleton::Clip{};
			clip.name = in_animation.name;
			for (auto const& in_channel : in_animation.channels) {
				if (!in_channel.target.node) { continue; }
				auto joint_it = map.find(*in_channel.target.node);
				if (joint_it == map.end()) { continue; }
				using Path = gltf2cpp::Animation::Path;
				auto channel = Skeleton::Channel{};
				auto const& sampler = in_animation.samplers[in_channel.sampler];
				if (sampler.interpolation == gltf2cpp::Interpolation::eCubicSpline) { continue; } // facade constraint
				auto const& input = in_root.accessors[sampler.input];
				assert(input.type == gltf2cpp::Accessor::Type::eScalar && input.component_type == gltf2cpp::ComponentType::eFloat);
				auto times = std::get<gltf2cpp::Accessor::Float>(input.data).span();
				auto const& output = in_root.accessors[sampler.output];
				assert(output.component_type == gltf2cpp::ComponentType::eFloat);
				auto const values = std::get<gltf2cpp::Accessor::Float>(output.data).span();
				channel.target = joint_it->second;
				switch (in_channel.target.path) {
				case Path::eTranslation:
				case Path::eScale: {
					assert(output.type == gltf2cpp::Accessor::Type::eVec3);
					auto vec = std::vector<glm::vec3>{};
					vec.resize(values.size() / 3);
					std::memcpy(vec.data(), values.data(), values.size_bytes());
					if (in_channel.target.path == Path::eScale) {
						channel.sampler = TransformAnimator::Scale{make_interpolator<glm::vec3>(times, vec, sampler.interpolation)};
					} else {
						channel.sampler = TransformAnimator::Translate{make_interpolator<glm::vec3>(times, vec, sampler.interpolation)};
					}
					break;
				}
				case Path::eRotation: {
					assert(output.type == gltf2cpp::Accessor::Type::eVec4);
					auto vec = std::vector<glm::quat>{};
					vec.resize(values.size() / 4);
					std::memcpy(vec.data(), values.data(), values.size_bytes());
					channel.sampler = TransformAnimator::Rotate{make_interpolator<glm::quat>(times, vec, sampler.interpolation)};
					break;
				}
				default:
					// TODO not implemented
					break;
				}
				clip.channels.push_back(std::move(channel));
			}
			if (!clip.channels.empty()) { skeleton.clips.push_back(std::move(clip)); }
		}

		if (in.inverse_bind_matrices) {
			auto const ibm = in_root.accessors[*in.inverse_bind_matrices].to_mat4();
			assert(ibm.size() >= in.joints.size());
			skeleton.inverse_bind_matrices.reserve(ibm.size());
			for (auto const& mat : ibm) { skeleton.inverse_bind_matrices.push_back(from(mat)); }
		} else {
			skeleton.inverse_bind_matrices = std::vector<glm::mat4x4>(skeleton.joints.size(), glm::identity<glm::mat4x4>());
		}

		auto asset = AssetSkeleton{std::move(skeleton)};
		auto fallback_name = in.name;
		if (fallback_name.empty() || fallback_name == "(Unnamed)") { fallback_name = in_root.meshes[*skin_node->mesh].name; }
		if (fallback_name.empty() || fallback_name == "(Unnamed)") { fallback_name = json_name; }
		fallback_name = fmt::format("{}_skeleton", fallback_name);

		auto uri = unique_filename(skeleton.name, ".json", fallback_name);
		asset.skeleton.name = fs::path{uri}.stem().string();
		auto json = dj::Json{};
		to_json(json, asset);
		auto mesh_uri = export_mesh(in_root.meshes[*skin_node->mesh], *skin_node->mesh, uri.string());
		json.to_file((out_path / uri).string().c_str());
		auto ret = uri.generic_string();
		logger::info("[Import] Skeleton [{}] imported", ret);
		exported.skeletons.insert_or_assign(index, ret);
		result.skeletons.push_back(uri.generic_string());
		return ret;
	}

	experiment::ImportedMeshes operator()() {
		for (auto [skin, index] : enumerate(in_root.skins)) { export_skeleton(skin, index); }
		for (auto [mesh, index] : enumerate(in_root.meshes)) { export_mesh(mesh, index, {}); }
		return std::move(result);
	}
};

dj::Json make_json(Rgba const& rgba) {
	auto ret = dj::Json{};
	ret["hex"] = to_hex(rgba);
	ret["intensity"] = rgba.intensity;
	return ret;
}

Rgba make_rgba(dj::Json const& json) { return to_rgba(json["hex"].as_string(), json["intensity"].as<float>(1.0f)); }

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
		out_kf["timestamp"] = in_kf.timestamp;
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
		out_kf.timestamp = in_kf["timestamp"].as<float>();
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
} // namespace

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

	if (in.compute_hash() != header.hash) {
		logger::error("[Import] Hash mismatch for BinGeometry. Corrupted file?");
		return false;
	}

	*this = std::move(in);
	return true;
}

Id<Texture> AssetLoader::load_texture(char const* path, ColourSpace colour_space) const {
	auto image = Image{path, fs::path{path}.filename().string()};
	if (!image) {
		logger::error("[Load] Failed to load image [{}]", path);
		return {};
	}
	auto const tci = TextureCreateInfo{
		.name = fs::path{path}.filename().string(),
		.mip_mapped = colour_space == ColourSpace::eSrgb,
		.colour_space = colour_space,
	};
	auto texture = graphics_device.make_texture(image, std::move(tci));
	logger::info("[Load] [{}] Texture loaded", texture.name());
	return mesh_resources.textures.add(std::move(texture)).first;
}

Id<StaticMesh> AssetLoader::try_load_static_mesh(char const* path) const {
	auto json = dj::Json::from_file(path);
	if (!json) {
		logger::error("[Load] Failed to open [{}]", path);
		return {};
	}
	if (json["asset_type"].as_string() != "mesh") {
		logger::error("[Load] JSON is not a Mesh [{}]", path);
		return {};
	}
	if (json["type"].as_string() != "static") { return {}; }
	return load_static_mesh(path, json);
}

Id<StaticMesh> AssetLoader::load_static_mesh(char const* path, dj::Json const& json) const {
	if (json["type"].as_string() != "static") {
		logger::error("[Load] JSON is not a StaticMesh [{}]", path);
		return {};
	}
	auto dir = fs::path{path}.parent_path();
	auto asset_path = [&dir](std::string_view const suffix) { return (dir / suffix).string(); };
	auto mesh = StaticMesh{};
	mesh.name = json["name"].as_string();
	for (auto const& in_primitive : json["primitives"].array_view()) {
		auto const& in_geometry = in_primitive["geometry"].as_string();
		auto const& in_material = in_primitive["material"].as_string();
		assert(!in_geometry.empty());
		auto bin_geometry = BinGeometry{};
		if (!bin_geometry.read((dir / in_geometry).string().c_str())) {
			logger::error("[Load] Failed to read bin geometry [{}]", in_geometry);
			continue;
		}
		auto asset_material = AssetMaterial{};
		if (!in_material.empty()) {
			auto json = dj::Json::from_file((dir / in_material).string().c_str());
			if (!json) {
				logger::error("[Load] Failed to open material JSON [{}]", in_material);
			} else {
				from_json(json, asset_material);
			}
		}
		auto material = LitMaterial{};
		material.albedo = asset_material.albedo;
		material.emissive_factor = asset_material.emissive_factor;
		material.metallic = asset_material.metallic;
		material.roughness = asset_material.roughness;
		material.alpha_cutoff = asset_material.alpha_cutoff;
		material.alpha_mode = asset_material.alpha_mode;
		material.shader = asset_material.shader;
		if (!asset_material.base_colour.value().empty()) {
			material.base_colour = load_texture(asset_path(asset_material.base_colour.value()).c_str(), ColourSpace::eSrgb);
		}
		if (!asset_material.emissive.value().empty()) {
			material.emissive = load_texture(asset_path(asset_material.emissive.value()).c_str(), ColourSpace::eSrgb);
		}
		if (!asset_material.roughness_metallic.value().empty()) {
			material.roughness_metallic = load_texture(asset_path(asset_material.roughness_metallic.value()).c_str(), ColourSpace::eLinear);
		}
		auto material_id = mesh_resources.materials.add(std::move(material)).first;
		auto geometry = graphics_device.make_mesh_geometry(bin_geometry.geometry, {bin_geometry.joints, bin_geometry.weights});
		mesh.primitives.push_back(MeshPrimitive{std::move(geometry), material_id});
	}
	logger::info("[Load] [{}] StaticMesh loaded", mesh.name);
	return mesh_resources.static_meshes.add(std::move(mesh)).first;
}

Id<Skeleton> AssetLoader::load_skeleton(char const* path) const {
	auto json = dj::Json::from_file(path);
	if (!json) {
		logger::error("[Load] Failed to open [{}]", path);
		return {};
	}
	if (json["asset_type"].as_string() != "skeleton") {
		logger::error("[Load] JSON is not a Skeleton [{}]", path);
		return {};
	}
	auto asset = AssetSkeleton{};
	from_json(json, asset);
	logger::info("[Load] [{}] Skeleton loaded", asset.skeleton.name);
	return mesh_resources.skeletons.add(std::move(asset.skeleton)).first;
}

Id<SkinnedMesh> AssetLoader::try_load_skinned_mesh(char const* path) const {
	auto json = dj::Json::from_file(path);
	if (!json) {
		logger::error("[Load] Failed to open [{}]", path);
		return {};
	}
	if (json["asset_type"].as_string() != "mesh") {
		logger::error("[Load] JSON is not a Mesh [{}]", path);
		return {};
	}
	if (json["type"].as_string() != "skinned") { return {}; }
	return load_skinned_mesh(path, json);
}

Id<SkinnedMesh> AssetLoader::load_skinned_mesh(char const* path, dj::Json const& json) const {
	if (json["type"].as_string() != "skinned") {
		logger::error("[Load] JSON is not a SkinnedMesh [{}]", path);
		return {};
	}
	auto dir = fs::path{path}.parent_path();
	auto asset_path = [&dir](std::string_view const suffix) { return (dir / suffix).string(); };
	auto mesh = SkinnedMesh{};
	mesh.name = json["name"].as_string();
	for (auto const& in_primitive : json["primitives"].array_view()) {
		auto const& in_geometry = in_primitive["geometry"].as_string();
		auto const& in_material = in_primitive["material"].as_string();
		assert(!in_geometry.empty());
		auto bin_geometry = BinGeometry{};
		if (!bin_geometry.read((dir / in_geometry).string().c_str())) {
			logger::error("[Load] Failed to read bin geometry [{}]", in_geometry);
			continue;
		}
		auto asset_material = AssetMaterial{};
		if (!in_material.empty()) {
			auto json = dj::Json::from_file((dir / in_material).string().c_str());
			if (!json) {
				logger::error("[Load] Failed to open material JSON [{}]", in_material);
			} else {
				from_json(json, asset_material);
			}
		}
		auto material = LitMaterial{};
		material.albedo = asset_material.albedo;
		material.emissive_factor = asset_material.emissive_factor;
		material.metallic = asset_material.metallic;
		material.roughness = asset_material.roughness;
		material.alpha_cutoff = asset_material.alpha_cutoff;
		material.alpha_mode = asset_material.alpha_mode;
		material.shader = asset_material.shader;
		if (!asset_material.base_colour.value().empty()) {
			material.base_colour = load_texture(asset_path(asset_material.base_colour.value()).c_str(), ColourSpace::eSrgb);
		}
		if (!asset_material.emissive.value().empty()) {
			material.emissive = load_texture(asset_path(asset_material.emissive.value()).c_str(), ColourSpace::eSrgb);
		}
		if (!asset_material.roughness_metallic.value().empty()) {
			material.roughness_metallic = load_texture(asset_path(asset_material.roughness_metallic.value()).c_str(), ColourSpace::eLinear);
		}
		auto material_id = mesh_resources.materials.add(std::move(material)).first;
		auto geometry = graphics_device.make_mesh_geometry(bin_geometry.geometry, {bin_geometry.joints, bin_geometry.weights});
		mesh.primitives.push_back(MeshPrimitive{std::move(geometry), material_id});
	}

	if (auto const& skeleton = json["skeleton"]) {
		auto const skeleton_uri = dir / skeleton.as_string();
		mesh.skeleton = load_skeleton(skeleton_uri.c_str());
	}
	logger::info("[Load] [{}] SkinnedMesh loaded", mesh.name);
	return mesh_resources.skinned_meshes.add(std::move(mesh)).first;
}

std::variant<std::monostate, Id<StaticMesh>, Id<SkinnedMesh>> AssetLoader::try_load_mesh(char const* path) const {
	if (auto ret = try_load_skinned_mesh(path)) { return ret; }
	if (auto ret = try_load_static_mesh(path)) { return ret; }
	logger::error("[Load] Failed to load mesh [{}]", path);
	return std::monostate{};
}
} // namespace experiment

void experiment::from_json(dj::Json const& json, AssetMaterial& out) {
	assert(json["asset_type"].as_string() == "material");
	out.albedo = make_rgba(json["albedo"]);
	out.emissive_factor = glm_vec_from_json<3>(json["emissive_factor"], out.emissive_factor);
	out.metallic = json["metallic"].as<float>(out.metallic);
	out.roughness = json["roughness"].as<float>(out.roughness);
	out.base_colour = std::string{json["base_colour"].as_string()};
	out.roughness_metallic = std::string{json["roughness_metallic"].as_string()};
	out.emissive = std::string{json["emissive"].as_string()};
	out.alpha_cutoff = json["alpha_cutoff"].as<float>(out.alpha_cutoff);
	out.shader = json["shader"].as_string(out.shader);
	out.name = json["name"].as_string();
}

void experiment::to_json(dj::Json& out, AssetMaterial const& asset) {
	out["asset_type"] = "material";
	out["albedo"] = make_json(asset.albedo);
	out["emissive_factor"] = make_json(asset.emissive_factor);
	out["metallic"] = asset.metallic;
	out["roughness"] = asset.roughness;
	if (!asset.base_colour.value().empty()) { out["base_colour"] = asset.base_colour.value(); }
	if (!asset.roughness_metallic.value().empty()) { out["roughness_metallic"] = asset.roughness_metallic.value(); }
	if (!asset.emissive.value().empty()) { out["emissive"] = asset.emissive.value(); }
	out["alpha_cutoff"] = asset.alpha_cutoff;
	out["shader"] = asset.shader;
	out["name"] = asset.name;
}

void experiment::from_json(dj::Json const& json, AssetMesh& out) {
	assert(json["asset_type"].as_string() == "mesh");
	out.type = json["type"].as_string() == "skinned" ? AssetMesh::Type::eSkinned : AssetMesh::Type::eStatic;
	for (auto const& in_primitive : json["primitives"].array_view()) {
		auto primitive = AssetMesh::Primitive{};
		primitive.geometry = std::string{in_primitive["geometry"].as_string()};
		primitive.material = std::string{in_primitive["material"].as_string()};
		out.primitives.push_back(std::move(primitive));
	}
	out.skeleton = std::string{json["skeleton"].as_string()};
	out.name = json["name"].as_string();
}

void experiment::to_json(dj::Json& out, AssetMesh const& asset) {
	out["asset_type"] = "mesh";
	out["type"] = asset.type == AssetMesh::Type::eSkinned ? "skinned" : "static";
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
	if (!asset.skeleton.value().empty()) { out["skeleton"] = asset.skeleton.value(); }
	out["name"] = asset.name;
}

void experiment::from_json(dj::Json const& json, AssetSkeleton& out) {
	assert(json["asset_type"].as_string() == "skeleton");
	out.skeleton.name = json["name"].as_string();
	for (auto const& in_ibm : json["inverse_bind_matrices"].array_view()) { out.skeleton.inverse_bind_matrices.push_back(glm_mat_from_json(in_ibm)); }
	for (auto const& in_joint : json["joints"].array_view()) {
		auto& out_joint = out.skeleton.joints.emplace_back();
		out_joint.name = in_joint["name"].as_string();
		out_joint.transform = from(glm_mat_from_json(in_joint["transform"]));
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
}

void experiment::to_json(dj::Json& out, AssetSkeleton const& asset) {
	out["asset_type"] = "skeleton";
	out["name"] = asset.skeleton.name;
	auto ibm = dj::Json{};
	for (auto const& in_ibm : asset.skeleton.inverse_bind_matrices) { ibm.push_back(make_json(in_ibm)); }
	out["inverse_bind_matrices"] = std::move(ibm);
	auto joints = dj::Json{};
	for (auto const& in_joint : asset.skeleton.joints) {
		auto out_joint = dj::Json{};
		out_joint["name"] = in_joint.name;
		out_joint["transform"] = make_json(in_joint.transform.matrix());
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
}

experiment::ImportedMeshes experiment::import_gltf_meshes(char const* gltf_path, char const* dest_dir) {
	if (!fs::is_regular_file(gltf_path)) {
		logger::error("[Import] Invalid GLTF file path [{}]", gltf_path);
		return {};
	}
	if (fs::is_regular_file(dest_dir)) {
		logger::error("[Import] Destination path [{}] occupied by a file", dest_dir);
		return {};
	}
	auto root = gltf2cpp::parse(gltf_path);
	if (!root) {
		logger::error("[Import] Failed to parse GLTF file [{}]", gltf_path);
		return {};
	}
	if (fs::is_directory(dest_dir)) {
		logger::info("[Import] Destination directory [{}] already exists", dest_dir);
	} else {
		fs::create_directories(dest_dir);
	}
	auto in_path = fs::path{gltf_path}.parent_path();
	auto json_name = fs::path{gltf_path}.filename().stem().string();
	return ExportMeshes{json_name, root, std::move(in_path), dest_dir}();
}
} // namespace levk
