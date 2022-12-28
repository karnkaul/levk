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

constexpr Sampler::Wrap from(gltf2cpp::Wrap const wrap) {
	switch (wrap) {
	case gltf2cpp::Wrap::eClampEdge: return Sampler::Wrap::eClampEdge;
	case gltf2cpp::Wrap::eMirrorRepeat: return Sampler::Wrap::eRepeat;
	default:
	case gltf2cpp::Wrap::eRepeat: return Sampler::Wrap::eRepeat;
	}
}

constexpr Sampler::Filter from(gltf2cpp::Filter const filter) {
	switch (filter) {
	default:
	case gltf2cpp::Filter::eLinear: return Sampler::Filter::eLinear;
	case gltf2cpp::Filter::eNearest: return Sampler::Filter::eNearest;
	}
}

constexpr Sampler from(gltf2cpp::Sampler const& sampler) {
	auto ret = Sampler{};
	ret.wrap_s = from(sampler.wrap_s);
	ret.wrap_t = from(sampler.wrap_t);
	if (sampler.min_filter) { ret.min = from(*sampler.min_filter); }
	if (sampler.mag_filter) { ret.mag = from(*sampler.mag_filter); }
	return ret;
}

constexpr Topology from(gltf2cpp::PrimitiveMode mode) {
	switch (mode) {
	case gltf2cpp::PrimitiveMode::ePoints: return Topology::ePointList;
	case gltf2cpp::PrimitiveMode::eLines: return Topology::eLineList;
	case gltf2cpp::PrimitiveMode::eLineStrip: return Topology::eLineStrip;
	case gltf2cpp::PrimitiveMode::eLineLoop: break;
	case gltf2cpp::PrimitiveMode::eTriangles: return Topology::eTriangleList;
	case gltf2cpp::PrimitiveMode::eTriangleStrip: return Topology::eTriangleStrip;
	case gltf2cpp::PrimitiveMode::eTriangleFan: return Topology::eTriangleFan;
	}
	throw Error{"Unsupported primitive mode: " + std::to_string(static_cast<int>(mode))};
}

glm::mat4x4 from(gltf2cpp::Mat4x4 const& in) {
	auto ret = glm::mat4x4{};
	std::memcpy(&ret[0], &in[0], sizeof(ret[0]));
	std::memcpy(&ret[1], &in[1], sizeof(ret[1]));
	std::memcpy(&ret[2], &in[2], sizeof(ret[2]));
	std::memcpy(&ret[3], &in[3], sizeof(ret[3]));
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
		[&ret](gltf2cpp::Mat4x4 const& mat) {
			auto m = from(mat);
			glm::vec3 scale{1.0f}, pos{}, skew{};
			glm::vec4 persp{};
			glm::quat orn{quat_identity_v};
			glm::decompose(m, scale, orn, pos, skew, persp);
			ret.set_position(pos);
			ret.set_orientation(orn);
			ret.set_scale(scale);
		},
	};
	std::visit(visitor, transform);
	return ret;
}

std::string to_hex(Rgba const& rgba) {
	auto ret = std::string{"#"};
	fmt::format_to(std::back_inserter(ret), "{:x}", rgba.channels.x);
	fmt::format_to(std::back_inserter(ret), "{:x}", rgba.channels.y);
	fmt::format_to(std::back_inserter(ret), "{:x}", rgba.channels.z);
	fmt::format_to(std::back_inserter(ret), "{:x}", rgba.channels.w);
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

struct GltfImporter {
	MeshResources& out_resources;
	GraphicsDevice& device;
	gltf2cpp::Root const& root;

	template <typename K, typename V>
	struct MapAndMutex {
		std::unordered_map<K, V> map{};
		std::mutex mutex{};
	};

	void import_all() {
		if (!root) { return; }
		for (auto [skin, index] : enumerate(root.skins)) { import_skin(skin, index); }
		for (auto [mesh, index] : enumerate(root.meshes)) { import_mesh(mesh, index); }
		// TODO: others
	}

	void import_skin(gltf2cpp::Skin const& in, std::size_t index) {
		auto skin_node = Ptr<gltf2cpp::Node const>{};
		for (auto& node : root.nodes) {
			if (node.skin && *node.skin == index) { skin_node = &node; }
		}
		if (!skin_node) { return; }

		assert(skin_node->mesh.has_value());
		auto lock = std::unique_lock{skeleton_map.mutex};
		if (skeleton_map.map.contains(index)) { return; }
		lock.unlock();

		auto [joints, map] = MapGltfNodesToJoints{}(in, root.nodes);
		auto skeleton = Skeleton{.joints = std::move(joints), .name = in.name};
		for (auto const& in_animation : root.animations) {
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
				auto const& input = root.accessors[sampler.input];
				assert(input.type == gltf2cpp::Accessor::Type::eScalar && input.component_type == gltf2cpp::ComponentType::eFloat);
				auto times = std::get<gltf2cpp::Accessor::Float>(input.data).span();
				auto const& output = root.accessors[sampler.output];
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
						channel.channel = TransformAnimator::Scale{make_interpolator<glm::vec3>(times, vec, sampler.interpolation)};
					} else {
						channel.channel = TransformAnimator::Translate{make_interpolator<glm::vec3>(times, vec, sampler.interpolation)};
					}
					break;
				}
				case Path::eRotation: {
					assert(output.type == gltf2cpp::Accessor::Type::eVec4);
					auto vec = std::vector<glm::quat>{};
					vec.resize(values.size() / 4);
					std::memcpy(vec.data(), values.data(), values.size_bytes());
					channel.channel = TransformAnimator::Rotate{make_interpolator<glm::quat>(times, vec, sampler.interpolation)};
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
			auto const ibm = root.accessors[*in.inverse_bind_matrices].to_mat4();
			assert(ibm.size() >= in.joints.size());
			skeleton.inverse_bind_matrices.reserve(ibm.size());
			for (auto const& mat : ibm) { skeleton.inverse_bind_matrices.push_back(from(mat)); }
		} else {
			skeleton.inverse_bind_matrices = std::vector<glm::mat4x4>(skeleton.joints.size(), glm::identity<glm::mat4x4>());
		}

		auto mesh_id = import_skinned_mesh(root.meshes[*skin_node->mesh], *skin_node->mesh);
		auto& mesh = out_resources.skinned_meshes.get(mesh_id);
		static_assert(IdSettableT<Skeleton>);
		auto skeleton_id = out_resources.skeletons.add(std::move(skeleton)).first;
		mesh.skeleton = skeleton_id;
		lock.lock();
		skeleton_map.map.insert_or_assign(index, mesh.skeleton);
	}

	Id<Material> import_material(gltf2cpp::Material const& in, std::size_t index) {
		auto lock = std::unique_lock{material_map.mutex};
		if (auto it = material_map.map.find(index); it != material_map.map.end()) { return it->second; }
		lock.unlock();
		auto material = LitMaterial{};
		material.albedo = Rgba::from(glm::vec4{in.pbr.base_color_factor[0], in.pbr.base_color_factor[1], in.pbr.base_color_factor[2], 1.0f});
		material.metallic = in.pbr.metallic_factor;
		material.roughness = in.pbr.roughness_factor;
		material.alpha_mode = from(in.alpha_mode);
		material.alpha_cutoff = in.alpha_cutoff;
		if (auto i = in.pbr.base_color_texture) { material.base_colour = import_texture(root.textures[i->texture], i->texture, ColourSpace::eSrgb); }
		if (auto i = in.pbr.metallic_roughness_texture) {
			material.roughness_metallic = import_texture(root.textures[i->texture], i->texture, ColourSpace::eLinear);
		}
		if (auto i = in.emissive_texture) { material.emissive = import_texture(root.textures[i->texture], i->texture, ColourSpace::eSrgb); }
		material.emissive_factor = {in.emissive_factor[0], in.emissive_factor[1], in.emissive_factor[2]};
		auto ret = out_resources.materials.add(std::move(material)).first;
		lock.lock();
		material_map.map.insert_or_assign(index, ret);
		return ret;
	}

	Id<Texture> import_texture(gltf2cpp::Texture const& in, std::size_t index, ColourSpace colour_space) {
		auto lock = std::unique_lock{texture_map.mutex};
		if (auto it = texture_map.map.find(index); it != texture_map.map.end()) { return it->second; }
		lock.unlock();
		auto sampler = Sampler{};
		if (in.sampler) { sampler = from(root.samplers[*in.sampler]); }
		auto tci = Texture::CreateInfo{
			in.name,
			colour_space == ColourSpace::eSrgb,
			colour_space,
			sampler,
		};
		auto const& image = load_image(root.images[in.source], in.source);
		auto ret = out_resources.textures.add(device.make_texture(image, tci)).first;
		{
			auto lock = std::scoped_lock{texture_metadata.mutex};
			texture_metadata.map.insert_or_assign(ret, TextureMetadata{tci.sampler, tci.colour_space, root.images[in.source].source_filename});
		}
		lock.lock();
		texture_map.map.insert_or_assign(index, ret);
		return ret;
	}

	Image const& load_image(gltf2cpp::Image const& in, std::size_t index) {
		auto lock = std::unique_lock{images.mutex};
		if (auto it = images.map.find(index); it != images.map.end()) { return it->second; }
		lock.unlock();
		auto image = Image{in.bytes.span(), in.name};
		lock.lock();
		auto [it, _] = images.map.insert_or_assign(index, std::move(image));
		return it->second;
	}

	Id<SkinnedMesh> import_skinned_mesh(gltf2cpp::Mesh const& in, std::size_t index) {
		auto lock = std::unique_lock{skinned_mesh_map.mutex};
		if (auto it = skinned_mesh_map.map.find(index); it != skinned_mesh_map.map.end()) { return it->second; }
		lock.unlock();
		auto mesh = SkinnedMesh{};
		mesh.name = in.name;
		for (auto const& primitive : in.primitives) {
			auto joints = std::vector<glm::uvec4>{};
			auto weights = std::vector<glm::vec4>{};
			if (!primitive.geometry.joints.empty()) {
				joints.resize(primitive.geometry.joints[0].size());
				std::memcpy(joints.data(), primitive.geometry.joints[0].data(), std::span{primitive.geometry.joints[0]}.size_bytes());
				weights.resize(primitive.geometry.weights[0].size());
				std::memcpy(weights.data(), primitive.geometry.weights[0].data(), std::span{primitive.geometry.weights[0]}.size_bytes());
			}
			auto material = Id<Material>{};
			if (primitive.material) { material = import_material(root.materials[*primitive.material], *primitive.material); }
			mesh.primitives.push_back(MeshPrimitive{
				.geometry = device.make_mesh_geometry(to_geometry(primitive), {joints, weights}),
				.material = material,
				.topology = from(primitive.mode),
			});
		}
		auto ret = out_resources.skinned_meshes.add(std::move(mesh)).first;
		lock.lock();
		skinned_mesh_map.map.insert_or_assign(index, ret);
		return ret;
	}

	Id<StaticMesh> import_static_mesh(gltf2cpp::Mesh const& in, std::size_t index) {
		auto lock = std::unique_lock{static_mesh_map.mutex};
		if (auto it = static_mesh_map.map.find(index); it != static_mesh_map.map.end()) { return it->second; }
		lock.unlock();
		auto mesh = StaticMesh{};
		mesh.name = in.name;
		for (auto const& primitive : in.primitives) {
			assert(primitive.geometry.joints.empty());
			auto material = Id<Material>{};
			if (primitive.material) { material = import_material(root.materials[*primitive.material], *primitive.material); }
			mesh.primitives.push_back(MeshPrimitive{
				.geometry = device.make_mesh_geometry(to_geometry(primitive)),
				.material = material,
				.topology = from(primitive.mode),
			});
		}
		auto ret = out_resources.static_meshes.add(std::move(mesh)).first;
		lock.lock();
		static_mesh_map.map.insert_or_assign(index, ret);
		return ret;
	}

	void import_mesh(gltf2cpp::Mesh const& in, std::size_t index) {
		auto lock = std::unique_lock{skinned_mesh_map.mutex};
		if (auto it = skinned_mesh_map.map.find(index); it != skinned_mesh_map.map.end()) { return; }
		lock.unlock();
		import_static_mesh(in, index);
	}

	MapAndMutex<gltf2cpp::Index<gltf2cpp::Texture>, Id<Texture>> texture_map{};
	MapAndMutex<gltf2cpp::Index<gltf2cpp::Material>, Id<Material>> material_map{};
	MapAndMutex<gltf2cpp::Index<gltf2cpp::Skin>, Id<Skeleton>> skeleton_map{};
	MapAndMutex<gltf2cpp::Index<gltf2cpp::Mesh>, Id<StaticMesh>> static_mesh_map{};
	MapAndMutex<gltf2cpp::Index<gltf2cpp::Mesh>, Id<SkinnedMesh>> skinned_mesh_map{};
	MapAndMutex<gltf2cpp::Index<gltf2cpp::Image>, Image> images{};
	MapAndMutex<std::size_t, TextureMetadata> texture_metadata{};
};
} // namespace
} // namespace experiment

experiment::ImportResult experiment::import_gltf(char const* gltf_path, GraphicsDevice& device, MeshResources& out_resources, ResourceMetadata& out_meta) {
	auto root = gltf2cpp::parse(gltf_path);
	if (!root) { return {}; }
	auto importer = GltfImporter{out_resources, device, root};
	importer.import_all();
	auto parent = fs::path{gltf_path}.parent_path();

	auto ret = ImportResult{};
	ret.added_textures.reserve(importer.texture_map.map.size());
	for (auto const& [_, id] : importer.texture_map.map) { ret.added_textures.push_back(id); }
	ret.added_materials.reserve(importer.material_map.map.size());
	for (auto const& [_, id] : importer.material_map.map) { ret.added_materials.push_back(id); }
	ret.added_skeletons.reserve(importer.skeleton_map.map.size());
	for (auto const& [_, id] : importer.skeleton_map.map) { ret.added_skeletons.push_back(id); }
	ret.added_static_meshes.reserve(importer.static_mesh_map.map.size());
	for (auto const& [_, id] : importer.static_mesh_map.map) { ret.added_static_meshes.push_back(id); }
	ret.added_skinned_meshes.reserve(importer.skinned_mesh_map.map.size());
	for (auto const& [_, id] : importer.skinned_mesh_map.map) { ret.added_skinned_meshes.push_back(id); }
	for (auto& [id, tsi] : importer.texture_metadata.map) {
		tsi.image_path = (parent / tsi.image_path).generic_string();
		out_meta.textures.insert_or_assign(id, tsi);
	}
	return ret;
}
} // namespace levk

namespace levk {
namespace experiment {
namespace {
struct ExportMeshes {
	gltf2cpp::Root& in_root;
	fs::path in_path;
	fs::path out_path;

	ImportedMeshes result{};

	struct {
		std::unordered_map<Index<gltf2cpp::Image>, std::string> images{};
		std::unordered_map<Index<gltf2cpp::Geometry>, std::string> geometries{};
		std::unordered_map<Index<gltf2cpp::Material>, std::string> materials{};
		std::unordered_map<Index<gltf2cpp::Mesh>, std::string> meshes{};
	} exported{};

	fs::path unique_filename(fs::path in, std::string_view fallback) {
		int index = 0;
		auto out = in;
		if (out.stem().empty()) {
			assert(!fallback.empty());
			in = fallback;
			out = fmt::format("{}_{}.{}", in.stem().string(), index, in.extension().string());
		}
		static constexpr auto max_iter_v = 100;
		auto path = out_path / out;
		for (; fs::exists(path) && index < max_iter_v; ++index) {
			out = fmt::format("{}_{}{}", in.stem().string(), index, in.extension().string());
			path = out_path / out;
		}
		assert(index < max_iter_v);
		return out;
	}

	std::string copy_image(gltf2cpp::Image const& in, std::size_t index) {
		assert(!in.source_filename.empty());
		if (auto it = exported.images.find(index); it != exported.images.end()) { return it->second; }
		auto uri = unique_filename(in.source_filename, "image.bin");
		auto dst = out_path / uri;
		fs::create_directories(dst.parent_path());
		fs::copy_file(in_path / in.source_filename, out_path / uri);
		auto ret = uri.generic_string();
		logger::info("[Import] Image [{}] copied from [{}]", ret, in.source_filename);
		exported.images.insert_or_assign(index, ret);
		return ret;
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
		auto filename = in.name + ".json";
		auto uri = unique_filename(filename, "material.json");
		auto json = dj::Json{};
		to_json(json, material);
		json.to_file((out_path / uri).string().c_str());
		auto ret = uri.generic_string();
		logger::info("[Import] Material [{}] imported", ret);
		exported.materials.insert_or_assign(index, ret);
		return ret;
	}

	AssetUri<BinGeometry> export_geometry(gltf2cpp::Mesh::Primitive const& in, std::size_t index, std::string_view mesh_name) {
		if (auto it = exported.geometries.find(index); it != exported.geometries.end()) { return it->second; }
		auto bin = BinGeometry{};
		bin.geometry = to_geometry(in);
		auto filename = fmt::format("{}_geometry{}.bin", mesh_name, index);
		auto uri = unique_filename(filename, filename);
		[[maybe_unused]] bool const res = bin.write((out_path / uri).string().c_str());
		assert(res);
		auto ret = uri.generic_string();
		logger::info("[Import] BinGeometry [{}] imported", ret);
		exported.geometries.insert_or_assign(index, ret);
		return ret;
	}

	experiment::ImportedMeshes operator()() {
		for (auto const& [in_mesh, mesh_index] : enumerate(in_root.meshes)) {
			if (exported.meshes.contains(mesh_index)) { continue; }
			auto out_mesh = AssetMesh{};
			for (auto const& [in_primitive, primitive_index] : enumerate(in_mesh.primitives)) {
				auto out_primitive = AssetMesh::Primitive{};
				if (!in_primitive.geometry.joints.empty()) {
					logger::warn("Skinned mesh not implemented");
					continue;
				}
				if (in_primitive.material) { out_primitive.material = export_material(in_root.materials[*in_primitive.material], *in_primitive.material); }
				out_primitive.geometry = export_geometry(in_primitive, primitive_index, in_mesh.name);
				out_mesh.primitives.push_back(std::move(out_primitive));
			}
			if (out_mesh.primitives.empty()) { continue; }
			out_mesh.name = in_mesh.name;
			auto filename = in_mesh.name + ".json";
			auto uri = unique_filename(filename, "mesh.json");
			auto json = dj::Json{};
			to_json(json, out_mesh);
			json.to_file((out_path / uri).string().c_str());
			auto ret = uri.generic_string();
			logger::info("[Import] Mesh [{}] imported", ret);
			exported.meshes.insert_or_assign(mesh_index, ret);
			result.meshes.push_back(uri.generic_string());
		}
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

dj::Json make_json(glm::vec3 const& vec3) {
	auto ret = dj::Json{};
	ret["x"] = vec3.x;
	ret["y"] = vec3.y;
	ret["z"] = vec3.z;
	return ret;
}

glm::vec3 make_vec3(dj::Json const& json, glm::vec3 const& fallback) {
	auto ret = glm::vec3{};
	ret.x = json["x"].as<float>(fallback.x);
	ret.y = json["y"].as<float>(fallback.y);
	ret.z = json["z"].as<float>(fallback.z);
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
} // namespace experiment

void experiment::from_json(dj::Json const& json, AssetMaterial& out) {
	out.albedo = make_rgba(json["albedo"]);
	out.emissive_factor = make_vec3(json["emissive_factor"], out.emissive_factor);
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
	return ExportMeshes{root, std::move(in_path), dest_dir}();
}

Id<Texture> experiment::load_texture(char const* path, GraphicsDevice& device, MeshResources& resources, ColourSpace colour_space) {
	auto image = Image{path, fs::path{path}.filename().string()};
	if (!image) {
		logger::error("[Load] Failed to load image [{}]", path);
		return {};
	}
	auto const tci = TextureCreateInfo{.mip_mapped = colour_space == ColourSpace::eSrgb, .colour_space = colour_space};
	return resources.textures.add(device.make_texture(image, tci)).first;
}

Id<StaticMesh> experiment::load_static_mesh(char const* path, GraphicsDevice& device, MeshResources& resources) {
	auto json = dj::Json::from_file(path);
	if (!json) {
		logger::error("[Load] Failed to open [{}]", path);
		return {};
	}
	if (json["type"].as_string() != "static") {
		logger::error("[Load] JSON is not a static mesh [{}]", path);
		return {};
	}
	auto dir = fs::path{path}.parent_path();
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
			material.base_colour = load_texture((dir / asset_material.base_colour.value()).string().c_str(), device, resources, ColourSpace::eSrgb);
		}
		if (!asset_material.emissive.value().empty()) {
			material.emissive = load_texture((dir / asset_material.emissive.value()).string().c_str(), device, resources, ColourSpace::eSrgb);
		}
		if (!asset_material.roughness_metallic.value().empty()) {
			material.roughness_metallic =
				load_texture((dir / asset_material.roughness_metallic.value()).string().c_str(), device, resources, ColourSpace::eLinear);
		}
		auto material_id = resources.materials.add(std::move(material)).first;
		auto geometry = device.make_mesh_geometry(bin_geometry.geometry, {bin_geometry.joints, bin_geometry.weights});
		mesh.primitives.push_back(MeshPrimitive{std::move(geometry), material_id});
	}
	return resources.static_meshes.add(std::move(mesh)).first;
}
} // namespace levk
