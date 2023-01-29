#include <glm/gtx/matrix_decompose.hpp>
#include <gltf2cpp/gltf2cpp.hpp>
#include <levk/asset/gltf_importer.hpp>
#include <levk/util/bool.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/error.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/visitor.hpp>
#include <levk/util/zip_ranges.hpp>
#include <filesystem>
#include <fstream>

namespace levk::asset {
namespace fs = std::filesystem;

namespace {
constexpr AlphaMode from(gltf2cpp::AlphaMode const mode) {
	switch (mode) {
	default:
	case gltf2cpp::AlphaMode::eOpaque: return AlphaMode::eOpaque;
	case gltf2cpp::AlphaMode::eBlend: return AlphaMode::eBlend;
	case gltf2cpp::AlphaMode::eMask: return AlphaMode::eMask;
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
	for (auto [t, v] : zip_ranges(times, values)) { ret.keyframes.push_back({v, Time{t}}); }
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
using Index = levk::Skeleton::Index<T>;

// Joints in a GLTF skin reference nodes in the GLTF global tree via their indices.
// To construct a skinned mesh skeleton and its animations, the tree needs to be trimmed to
// only the nodes relevant to the skin, and node indices correspondingly retargeted.
// A skeleton instance will then add one node per joint to the engine's scene tree, copy
// the animations and rewrite each animator target to match its corresponding Id<Node>.
// The order of joints after all these transformations must be identical to the source data,
// hence the [Index => *] unordered_map invasion.
struct MapGltfNodesToJoints {
	struct Entry {
		levk::Skeleton::Joint joint{};
		Index<levk::Skeleton::Joint> index{};
	};

	struct Result {
		std::vector<levk::Skeleton::Joint> joints{};
		std::unordered_map<Index<gltf2cpp::Node>, Index<levk::Skeleton::Joint>> map{};
	};

	std::unordered_map<Index<gltf2cpp::Node>, Entry> map{};
	std::vector<levk::Skeleton::Joint> joints{};

	std::span<gltf2cpp::Node const> nodes{};

	void add_node_and_children(Index<Node> node_idx, std::optional<Index<levk::Skeleton::Joint>> parent = {}) {
		if (map.contains(node_idx)) { return; }
		auto const& node = nodes[node_idx];
		auto entry = Entry{};
		entry.joint.name = node.name;
		entry.joint.transform = asset::from(node.transform);
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

struct GltfResource {
	struct {
		std::string in{};
		std::string out{};
		std::string log{};
	} name{};
	std::size_t index{};
};

std::vector<GltfMesh> make_gltf_mesh_list(dj::Json const& json) {
	auto ret = std::vector<GltfMesh>{};
	for (auto const [mesh, index] : enumerate(json["meshes"].array_view())) {
		auto const& primitives = mesh["primitives"];
		if (!primitives || primitives.array_view().empty()) { continue; }
		auto const& attributes = primitives[0]["attributes"];
		if (attributes.object_view().empty()) { continue; }
		auto const mesh_type = attributes.contains("JOINTS_0") ? MeshType::eSkinned : MeshType::eStatic;
		ret.push_back(GltfMesh{mesh["name"].as<std::string>(), mesh_type, index});
	}
	return ret;
}

std::vector<GltfScene> make_gltf_scene_list(dj::Json const& json) {
	auto ret = std::vector<GltfScene>{};
	for (auto const [scene, index] : enumerate(json["scenes"].array_view())) { ret.push_back(GltfScene{scene["name"].as<std::string>(), index}); }
	return ret;
}

std::string log_name(std::string_view in, std::size_t index) {
	auto ret = fmt::format("{}", index);
	if (!in.empty()) { fmt::format_to(std::back_inserter(ret), " - {}", in); }
	return ret;
}

std::string asset_name(std::string_view in, std::string_view type, std::size_t index, Bool allow_unnamed) {
	auto ret = std::string{};
	ret.reserve(128);
	if (!in.empty() && (allow_unnamed || in != "(Unnamed)")) { fmt::format_to(std::back_inserter(ret), "{}.", in); }
	fmt::format_to(std::back_inserter(ret), "{}_{}", type, index);
	return ret;
}

GltfResource make_resource(std::string_view in, std::string_view type, std::size_t index) {
	auto ret = GltfResource{.index = index};
	ret.name.in = in;
	ret.name.out = asset_name(in, type, index, {false});
	ret.name.log = log_name(in, index);
	return ret;
}

struct GltfExporter {
	LogDispatch const& import_logger;
	gltf2cpp::Root const& in_root;
	fs::path in_dir;
	fs::path out_dir;

	struct {
		std::unordered_map<Index<gltf2cpp::Image>, std::string> images{};
		std::unordered_map<Index<gltf2cpp::Geometry>, std::string> geometries{};
		std::unordered_map<Index<gltf2cpp::Material>, std::string> materials{};
		std::unordered_map<Index<gltf2cpp::Mesh>, std::string> meshes{};
		std::unordered_map<Index<gltf2cpp::Skin>, std::string> skeletons{};
	} exported{};

	std::optional<Index<gltf2cpp::Skin>> find_skin(GltfResource const& resource) const {
		for (auto const [node, index] : enumerate(in_root.nodes)) {
			if (node.mesh && *node.mesh == resource.index) {
				if (!node.skin) {
					import_logger.error("[Import] GLTF node [{}] with SkinnedMesh [{}] does not have a GLTF Skin", index, resource.name.log);
					return {};
				}
				return *node.skin;
			}
		}
		import_logger.error("[Import] No Skin found for SkinnedMesh [{}]", resource.name.log);
		return {};
	}

	std::string copy_image(gltf2cpp::Image const& in, std::size_t index) {
		assert(!in.source_filename.empty());
		if (auto it = exported.images.find(index); it != exported.images.end()) { return it->second; }
		auto uri = fs::path{in.source_filename}.generic_string();
		auto dst = out_dir / uri;
		if (fs::exists(dst)) {
			import_logger.info("[Import] Import target exists, reusing: [{}]", dst.generic_string());
			return uri;
		}
		fs::create_directories(dst.parent_path());
		if (fs::exists(dst)) {
			import_logger.warn("[Import] Overwriting existing file: [{}]", dst.generic_string());
			fs::remove(dst);
		}
		fs::copy_file(in_dir / in.source_filename, out_dir / uri);
		import_logger.info("[Import] Image [{}] copied", uri);
		exported.images.insert_or_assign(index, uri);
		return uri;
	}

	std::string copy_image(gltf2cpp::Texture const& in, std::size_t index) { return copy_image(in_root.images[in.source], index); }

	Uri<Material> export_material(GltfResource const& resource) {
		if (auto it = exported.materials.find(resource.index); it != exported.materials.end()) { return it->second; }
		auto uri = fmt::format("{}.json", resource.name.out);
		auto dst = out_dir / uri;
		if (fs::exists(dst)) {
			import_logger.info("[Import] Import target exists, reusing: [{}]", dst.generic_string());
			return uri;
		}
		auto const& in = in_root.materials[resource.index];
		auto material = asset::Material{};
		material.albedo = Rgba::from(glm::vec4{in.pbr.base_color_factor[0], in.pbr.base_color_factor[1], in.pbr.base_color_factor[2], 1.0f});
		material.metallic = in.pbr.metallic_factor;
		material.roughness = in.pbr.roughness_factor;
		material.alpha_mode = from(in.alpha_mode);
		material.alpha_cutoff = in.alpha_cutoff;
		material.name = in.name;
		auto& textures = material.textures.textures;
		if (auto i = in.pbr.base_color_texture) { textures[0] = {copy_image(in_root.textures[i->texture], i->texture), ColourSpace::eSrgb}; }
		if (auto i = in.pbr.metallic_roughness_texture) { textures[1] = {copy_image(in_root.textures[i->texture], i->texture), ColourSpace::eLinear}; }
		if (auto i = in.emissive_texture) { textures[2] = {copy_image(in_root.textures[i->texture], i->texture), ColourSpace::eSrgb}; }
		material.emissive_factor = {in.emissive_factor[0], in.emissive_factor[1], in.emissive_factor[2]};
		auto json = dj::Json{};
		to_json(json, material);
		if (fs::exists(dst)) {
			import_logger.warn("[Import] Overwriting existing file: [{}]", dst.generic_string());
			fs::remove(dst);
		}
		json.to_file(dst.string().c_str());
		import_logger.info("[Import] Material [{}] imported", uri);
		exported.materials.insert_or_assign(resource.index, uri);
		return uri;
	}

	Uri<BinGeometry> export_geometry(gltf2cpp::Mesh const& in, std::size_t primitive_index, std::size_t mesh_index, std::vector<glm::uvec4> joints,
									 std::vector<glm::vec4> weights) {
		if (auto it = exported.geometries.find(primitive_index); it != exported.geometries.end()) { return it->second; }
		auto uri = fmt::format("mesh_{}.geometry_{}.bin", mesh_index, primitive_index);
		auto dst = out_dir / uri;
		if (fs::exists(dst)) {
			import_logger.info("[Import] Import target exists, reusing: [{}]", dst.generic_string());
			return uri;
		}
		auto bin = BinGeometry{};
		bin.geometry = to_geometry(in.primitives[primitive_index]);
		bin.joints = std::move(joints);
		bin.weights = std::move(weights);
		[[maybe_unused]] bool const res = bin.write(dst.string().c_str());
		assert(res);
		import_logger.info("[Import] BinGeometry [{}] imported", uri);
		exported.geometries.insert_or_assign(primitive_index, uri);
		return uri;
	}

	Uri<Mesh> export_mesh(GltfResource const& resource) {
		auto uri = fmt::format("{}.json", resource.name.out);
		auto dst = out_dir / uri;
		if (fs::exists(dst)) {
			import_logger.info("[Import] Import target exists, reusing: [{}]", dst.generic_string());
			return uri;
		}
		auto out_mesh = Mesh{.type = Mesh::Type::eStatic};
		auto const& in_mesh = in_root.meshes[resource.index];
		for (auto const& [in_primitive, primitive_index] : enumerate(in_mesh.primitives)) {
			auto out_primitive = Mesh::Primitive{};
			if (in_primitive.material) {
				auto const& in_material = in_root.materials[*in_primitive.material];
				out_primitive.material = export_material(make_resource(in_material.name, "material", *in_primitive.material));
			}
			if (!in_primitive.geometry.joints.empty()) { out_mesh.type = Mesh::Type::eSkinned; }
			auto joints = std::vector<glm::uvec4>{};
			auto weights = std::vector<glm::vec4>{};
			if (!in_primitive.geometry.joints.empty()) {
				joints.resize(in_primitive.geometry.joints[0].size());
				std::memcpy(joints.data(), in_primitive.geometry.joints[0].data(), std::span{in_primitive.geometry.joints[0]}.size_bytes());
				weights.resize(in_primitive.geometry.weights[0].size());
				std::memcpy(weights.data(), in_primitive.geometry.weights[0].data(), std::span{in_primitive.geometry.weights[0]}.size_bytes());
			}
			out_primitive.geometry = export_geometry(in_mesh, primitive_index, resource.index, std::move(joints), std::move(weights));
			out_mesh.primitives.push_back(std::move(out_primitive));
		}
		if (out_mesh.primitives.empty()) {
			import_logger.warn("[Import] Mesh [{}] has no primitives, skipping", resource.name.log);
			return {};
		}
		if (out_mesh.type == Mesh::Type::eSkinned) {
			auto const skin = find_skin(resource);
			if (!skin) {
				import_logger.error("[Import] Failed to locate GLTF Skin for SkinnedMesh [{}]", resource.name.log);
				return {};
			}
			if (*skin >= in_root.skins.size()) {
				import_logger.error("[Import] SkinnedMesh [{}] has an invalid a GLTF Skin index: [{}]", resource.name.log, *skin);
				return {};
			}
			auto const& in_skin = in_root.skins[*skin];
			if (in_skin.inverse_bind_matrices) {
				auto const ibm = in_root.accessors[*in_skin.inverse_bind_matrices].to_mat4();
				assert(ibm.size() >= in_skin.joints.size());
				out_mesh.inverse_bind_matrices.reserve(ibm.size());
				for (auto const& mat : ibm) { out_mesh.inverse_bind_matrices.push_back(from(mat)); }
			} else {
				out_mesh.inverse_bind_matrices = std::vector<glm::mat4>(in_skin.joints.size(), glm::identity<glm::mat4x4>());
			}
			out_mesh.skeleton = export_skeleton(make_resource(in_skin.name, "skeleton", *skin));
		}
		out_mesh.name = fs::path{resource.name.out}.stem().string();
		auto json = dj::Json{};
		to_json(json, out_mesh);
		if (fs::exists(dst)) {
			import_logger.warn("[Import] Overwriting existing file: [{}]", dst.generic_string());
			fs::remove(dst);
		}
		json.to_file(dst.string().c_str());
		import_logger.info("[Import] Mesh [{}] imported", uri);
		return uri;
	}

	Uri<asset::BinSkeletalAnimation> export_skeletal_animation(GltfResource const& resource, asset::BinSkeletalAnimation const& animation) {
		auto uri = fmt::format("{}.bin", resource.name.out);
		auto dst = out_dir / uri;
		if (fs::exists(dst)) {
			import_logger.info("[Import] Import target exists, reusing: [{}]", dst.generic_string());
			return uri;
		}
		if (!animation.write(dst.generic_string().c_str())) {
			import_logger.error("[Import] Failed to import Skeletal Animation: [{}]", uri);
			return {};
		}
		import_logger.info("[Import] Skeleton Animation [{}] imported", uri);
		return uri;
	}

	Uri<Skeleton> export_skeleton(GltfResource const& resource_skin) {
		auto uri = fmt::format("{}.json", resource_skin.name.out);
		auto dst = out_dir / uri;
		if (fs::exists(dst)) {
			import_logger.info("[Import] Import target exists, reusing: [{}]", dst.generic_string());
			return uri;
		}

		auto skin_node = Ptr<gltf2cpp::Node const>{};
		for (auto& node : in_root.nodes) {
			if (node.skin && *node.skin == resource_skin.index) { skin_node = &node; }
		}
		if (!skin_node) { return {}; }

		assert(skin_node->mesh.has_value());
		if (auto it = exported.skeletons.find(resource_skin.index); it != exported.skeletons.end()) { return it->second; }

		auto const& in = in_root.skins[resource_skin.index];
		auto [joints, map] = MapGltfNodesToJoints{}(in, in_root.nodes);

		auto animations = std::vector<BinSkeletalAnimation>{};
		for (auto const& in_animation : in_root.animations) {
			auto out_animation = BinSkeletalAnimation{};
			out_animation.name = in_animation.name;
			for (auto const& in_channel : in_animation.channels) {
				if (!in_channel.target.node) { continue; }
				auto joint_it = map.find(*in_channel.target.node);
				if (joint_it == map.end()) { continue; }
				using Path = gltf2cpp::Animation::Path;
				auto out_sampler = BinSkeletalAnimation::Sampler{};
				auto const& in_sampler = in_animation.samplers[in_channel.sampler];
				if (in_sampler.interpolation == gltf2cpp::Interpolation::eCubicSpline) { continue; } // facade constraint
				auto const& input = in_root.accessors[in_sampler.input];
				assert(input.type == gltf2cpp::Accessor::Type::eScalar && input.component_type == gltf2cpp::ComponentType::eFloat);
				auto times = std::get<gltf2cpp::Accessor::Float>(input.data).span();
				auto const& output = in_root.accessors[in_sampler.output];
				assert(output.component_type == gltf2cpp::ComponentType::eFloat);
				auto const values = std::get<gltf2cpp::Accessor::Float>(output.data).span();
				switch (in_channel.target.path) {
				case Path::eTranslation:
				case Path::eScale: {
					assert(output.type == gltf2cpp::Accessor::Type::eVec3);
					auto vec = std::vector<glm::vec3>{};
					vec.resize(values.size() / 3);
					std::memcpy(vec.data(), values.data(), values.size_bytes());
					if (in_channel.target.path == Path::eScale) {
						out_sampler = TransformAnimation::Scale{make_interpolator<glm::vec3>(times, vec, in_sampler.interpolation)};
					} else {
						out_sampler = TransformAnimation::Translate{make_interpolator<glm::vec3>(times, vec, in_sampler.interpolation)};
					}
					break;
				}
				case Path::eRotation: {
					assert(output.type == gltf2cpp::Accessor::Type::eVec4);
					auto vec = std::vector<glm::quat>{};
					vec.resize(values.size() / 4);
					std::memcpy(vec.data(), values.data(), values.size_bytes());
					out_sampler = TransformAnimation::Rotate{make_interpolator<glm::quat>(times, vec, in_sampler.interpolation)};
					break;
				}
				default:
					// TODO: not implemented
					continue;
				}

				out_animation.samplers.push_back(std::move(out_sampler));
				out_animation.target_joints.push_back(joint_it->second);
			}
			if (!out_animation.samplers.empty()) {
				assert(out_animation.samplers.size() == out_animation.target_joints.size());
				animations.push_back(std::move(out_animation));
			}
		}

		auto asset = Skeleton{.joints = std::move(joints)};
		for (auto const [in_animation, index] : enumerate(animations)) {
			auto const animation_prefix = make_resource(in_animation.name, "animation", index);
			asset.animations.push_back(export_skeletal_animation(animation_prefix, in_animation));
		}

		asset.name = fs::path{resource_skin.name.out}.stem().string();
		auto json = dj::Json{};
		to_json(json, asset);
		if (fs::exists(dst)) {
			import_logger.warn("[Import] Overwriting existing file: [{}]", dst.generic_string());
			fs::remove(dst);
		}
		json.to_file(dst.string().c_str());
		import_logger.info("[Import] Skeleton [{}] imported", uri);
		exported.skeletons.insert_or_assign(resource_skin.index, uri);
		return uri;
	}
};
} // namespace

void LogDispatch::print(logger::Level level, std::string message) const {
	if (silenced[static_cast<std::size_t>(level)]) { return; }
	auto const pipe = level == logger::Level::eError ? logger::Pipe::eStdErr : logger::Pipe::eStdOut;
	log_to(pipe, {std::move(message), level});
}

GltfImporter::List GltfImporter::peek(std::string gltf_path, LogDispatch const& import_logger) {
	auto ret = GltfImporter::List{};
	if (!fs::is_regular_file(gltf_path)) {
		import_logger.error("[Import] Invalid GLTF file path [{}]", gltf_path);
		return ret;
	}
	auto json = dj::Json::from_file(gltf_path.c_str());
	ret.meshes = make_gltf_mesh_list(json);
	ret.scenes = make_gltf_scene_list(json);
	ret.gltf_path = std::move(gltf_path);
	return ret;
}

GltfImporter GltfImporter::List::importer(std::string dst_dir, LogDispatch import_logger) const {
	auto ret = GltfImporter{std::move(import_logger)};
	if (gltf_path.empty()) {
		ret.import_logger.error("[Import] Empty GLTF file path");
		return ret;
	}
	if (dst_dir.empty()) {
		ret.import_logger.error("[Import] Empty destination directory");
		return ret;
	}
	ret.root = gltf2cpp::parse(gltf_path.c_str());
	if (!ret.root) {
		ret.import_logger.error("[Import] Failed to parse GLTF [{}]", gltf_path);
		return ret;
	}
	ret.src_dir = fs::path{gltf_path}.parent_path().string();
	ret.dst_dir = std::move(dst_dir);
	return ret;
}

Uri<Mesh> GltfImporter::import_mesh(GltfMesh const& mesh) const {
	if (fs::exists(dst_dir)) {
		import_logger.warn("[Import] Destination directory [{}] exists, assets may be overwritten", dst_dir);
	} else {
		fs::create_directories(dst_dir);
	}
	return GltfExporter{import_logger, root, src_dir, dst_dir}.export_mesh(make_resource(mesh.name, "mesh", mesh.index));
}
} // namespace levk::asset

levk::Transform levk::asset::from(gltf2cpp::Transform const& transform) {
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
