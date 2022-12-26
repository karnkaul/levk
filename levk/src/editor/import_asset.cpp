#include <glm/gtx/matrix_decompose.hpp>
#include <gltf2cpp/gltf2cpp.hpp>
#include <levk/editor/import_asset.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/error.hpp>
#include <levk/util/visitor.hpp>
#include <levk/util/zip_ranges.hpp>
#include <filesystem>
#include <fstream>

namespace levk {
namespace fs = std::filesystem;

namespace editor {
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
} // namespace editor

editor::ImportResult editor::import_gltf(char const* gltf_path, GraphicsDevice& device, MeshResources& out_resources, ResourceMetadata& out_meta) {
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
