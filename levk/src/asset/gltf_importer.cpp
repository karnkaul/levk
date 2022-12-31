#include <glm/gtx/matrix_decompose.hpp>
#include <gltf2cpp/gltf2cpp.hpp>
#include <levk/asset/gltf_importer.hpp>
#include <levk/util/binary_file.hpp>
#include <levk/util/bool.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/error.hpp>
#include <levk/util/hash_combine.hpp>
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

GltfAsset make_gltf_asset_view(std::string_view gltf_name, std::size_t index, std::string_view asset_type, Bool index_suffix = {true}) {
	assert(!asset_type.empty());
	auto ret = GltfAsset{.gltf_name = std::string{gltf_name}, .index = index};
	ret.asset_name = gltf_name.empty() ? asset_type : ret.gltf_name;
	if (index_suffix) { fmt::format_to(std::back_inserter(ret.asset_name), "_{}", index); }
	if (!gltf_name.empty()) { fmt::format_to(std::back_inserter(ret.asset_name), ".{}", asset_type); }
	return ret;
}

GltfAsset::List make_gltf_asset_view_list(dj::Json const& json) {
	auto ret = GltfAsset::List{};
	for (auto const [mesh, index] : enumerate(json["meshes"].array_view())) {
		auto const& primitives = mesh["primitives"];
		if (!primitives || primitives.array_view().empty()) { continue; }
		auto const& attributes = primitives[0]["attributes"];
		if (attributes.object_view().empty()) { continue; }
		auto asset_view = make_gltf_asset_view(mesh["name"].as_string(), index, "mesh", {false});
		if (attributes.contains("JOINTS_0")) {
			ret.skinned_meshes.push_back(std::move(asset_view));
		} else {
			ret.static_meshes.push_back(std::move(asset_view));
		}
	}
	for (auto const [skin, index] : enumerate(json["skins"].array_view())) {
		ret.skeletons.push_back(make_gltf_asset_view(skin["name"].as_string(), index, "skeleton", {true}));
	}
	return ret;
}

struct GltfExporter {
	logger::Dispatch const& import_logger;
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

	std::optional<Index<gltf2cpp::Skin>> find_skin(GltfAsset const& av_mesh) const {
		for (auto const [node, index] : enumerate(in_root.nodes)) {
			if (node.mesh && *node.mesh == av_mesh.index) {
				if (!node.skin) {
					import_logger.error("[Import] GLTF node [{}] with SkinnedMesh [{}] does not have a GLTF Skin", index, av_mesh.asset_name);
					return {};
				}
				return *node.skin;
			}
		}
		import_logger.error("[Import] No Skin found for SkinnedMesh [{}]", av_mesh.asset_name);
		return {};
	}

	std::string copy_image(gltf2cpp::Image const& in, std::size_t index) {
		assert(!in.source_filename.empty());
		if (auto it = exported.images.find(index); it != exported.images.end()) { return it->second; }
		auto uri = fs::path{in.source_filename}.generic_string();
		auto dst = out_dir / uri;
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

	Uri<Material> export_material(GltfAsset const& av_material) {
		if (auto it = exported.materials.find(av_material.index); it != exported.materials.end()) { return it->second; }
		auto const& in = in_root.materials[av_material.index];
		auto material = asset::Material{};
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
		auto uri = fmt::format("{}.json", av_material.asset_name);
		auto json = dj::Json{};
		to_json(json, material);
		auto dst = out_dir / uri;
		if (fs::exists(dst)) {
			import_logger.warn("[Import] Overwriting existing file: [{}]", dst.generic_string());
			fs::remove(dst);
		}
		json.to_file(dst.string().c_str());
		import_logger.info("[Import] Material [{}] imported", uri);
		exported.materials.insert_or_assign(av_material.index, uri);
		return uri;
	}

	Uri<BinGeometry> export_geometry(gltf2cpp::Mesh::Primitive const& in, std::size_t index, std::vector<glm::uvec4> joints, std::vector<glm::vec4> weights) {
		if (auto it = exported.geometries.find(index); it != exported.geometries.end()) { return it->second; }
		auto bin = BinGeometry{};
		bin.geometry = to_geometry(in);
		bin.joints = std::move(joints);
		bin.weights = std::move(weights);
		auto uri = fmt::format("{}.bin", make_gltf_asset_view({}, index, "geometry").asset_name);
		[[maybe_unused]] bool const res = bin.write((out_dir / uri).string().c_str());
		assert(res);
		import_logger.info("[Import] BinGeometry [{}] imported", uri);
		exported.geometries.insert_or_assign(index, uri);
		return uri;
	}

	Uri<Mesh> export_mesh(GltfAsset const& av_mesh) {
		auto out_mesh = Mesh{.type = Mesh::Type::eStatic};
		auto const& in_mesh = in_root.meshes[av_mesh.index];
		for (auto const& [in_primitive, primitive_index] : enumerate(in_mesh.primitives)) {
			auto out_primitive = Mesh::Primitive{};
			if (in_primitive.material) {
				auto const& in_material = in_root.materials[*in_primitive.material];
				out_primitive.material = export_material(make_gltf_asset_view(in_material.name, *in_primitive.material, "material"));
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
			out_primitive.geometry = export_geometry(in_primitive, primitive_index, std::move(joints), std::move(weights));
			out_mesh.primitives.push_back(std::move(out_primitive));
		}
		if (out_mesh.primitives.empty()) {
			import_logger.warn("[Import] Mesh [{}] has no primitives, skipping", av_mesh.asset_name);
			return {};
		}
		if (out_mesh.type == Mesh::Type::eSkinned) {
			auto const skin = find_skin(av_mesh);
			if (!skin) {
				import_logger.error("[Import] Failed to locate GLTF Skin for SkinnedMesh [{}]", av_mesh.asset_name);
				return {};
			}
			if (*skin >= in_root.skins.size()) {
				import_logger.error("[Import] SkinnedMesh [{}] has an invalid a GLTF Skin index: [{}]", av_mesh.asset_name, *skin);
				return {};
			}
			auto const& in_skin = in_root.skins[*skin];
			out_mesh.skeleton = export_skeleton(make_gltf_asset_view(in_skin.name, *skin, "skeleton"));
		}
		auto uri = fmt::format("{}.json", av_mesh.asset_name);
		out_mesh.name = fs::path{av_mesh.asset_name}.stem().string();
		auto json = dj::Json{};
		to_json(json, out_mesh);
		auto dst = out_dir / uri;
		if (fs::exists(dst)) {
			import_logger.warn("[Import] Overwriting existing file: [{}]", dst.generic_string());
			fs::remove(dst);
		}
		json.to_file(dst.string().c_str());
		import_logger.info("[Import] Mesh [{}] imported", uri);
		return uri;
	}

	Uri<Skeleton> export_skeleton(GltfAsset const& av_skin) {
		auto skin_node = Ptr<gltf2cpp::Node const>{};
		for (auto& node : in_root.nodes) {
			if (node.skin && *node.skin == av_skin.index) { skin_node = &node; }
		}
		if (!skin_node) { return {}; }

		assert(skin_node->mesh.has_value());
		if (auto it = exported.skeletons.find(av_skin.index); it != exported.skeletons.end()) { return it->second; }

		auto const& in = in_root.skins[av_skin.index];
		auto [joints, map] = MapGltfNodesToJoints{}(in, in_root.nodes);
		auto skeleton = levk::Skeleton{.joints = std::move(joints)};
		for (auto const& in_animation : in_root.animations) {
			auto clip = levk::Skeleton::Clip{};
			clip.name = in_animation.name;
			for (auto const& in_channel : in_animation.channels) {
				if (!in_channel.target.node) { continue; }
				auto joint_it = map.find(*in_channel.target.node);
				if (joint_it == map.end()) { continue; }
				using Path = gltf2cpp::Animation::Path;
				auto channel = levk::Skeleton::Channel{};
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

		auto asset = Skeleton{std::move(skeleton)};
		auto uri = fmt::format("{}.json", av_skin.asset_name);
		asset.skeleton.name = fs::path{av_skin.asset_name}.stem().string();
		auto json = dj::Json{};
		to_json(json, asset);
		auto dst = out_dir / uri;
		if (fs::exists(dst)) {
			import_logger.warn("[Import] Overwriting existing file: [{}]", dst.generic_string());
			fs::remove(dst);
		}
		json.to_file(dst.string().c_str());
		import_logger.info("[Import] Skeleton [{}] imported", uri);
		exported.skeletons.insert_or_assign(av_skin.index, uri);
		return uri;
	}
};
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

	if (in.compute_hash() != header.hash) { return false; }

	*this = std::move(in);
	return true;
}

GltfImporter::List GltfImporter::peek(std::string gltf_path, logger::Dispatch const& import_logger) {
	auto ret = GltfImporter::List{};
	ret.import_logger = import_logger;
	if (!fs::is_regular_file(gltf_path)) {
		import_logger.error("[Import] Invalid GLTF file path [{}]", gltf_path);
		return ret;
	}
	static_cast<GltfAsset::List&>(ret) = make_gltf_asset_view_list(dj::Json::from_file(gltf_path.c_str()));
	ret.gltf_path = std::move(gltf_path);
	return ret;
}

GltfImporter GltfImporter::List::importer(std::string dest_dir) const {
	auto ret = GltfImporter{import_logger};
	if (gltf_path.empty()) {
		import_logger.error("[Import] Empty GLTF file path");
		return ret;
	}
	if (dest_dir.empty()) {
		import_logger.error("[Import] Empty destination directory");
		return ret;
	}
	ret.root = gltf2cpp::parse(gltf_path.c_str());
	if (!ret.root) {
		import_logger.error("[Import] Failed to parse GLTF [{}]", gltf_path);
		return ret;
	}
	if (fs::exists(dest_dir)) {
		import_logger.warn("[Import] Destination directory [{}] exists, assets may be overwritten", dest_dir);
	} else {
		fs::create_directories(dest_dir);
	}
	ret.src_dir = fs::path{gltf_path}.parent_path().string();
	ret.dest_dir = std::move(dest_dir);
	return ret;
}

Uri<Mesh> GltfImporter::import_mesh(GltfAsset const& mesh) const { return GltfExporter{import_logger, root, src_dir, dest_dir}.export_mesh(mesh); }
} // namespace levk::asset
