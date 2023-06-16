#include <glm/gtx/matrix_decompose.hpp>
#include <legsmi/legsmi.hpp>
#include <levk/asset/asset_io.hpp>
#include <levk/io/serializer.hpp>
#include <levk/level/attachments.hpp>
#include <levk/scene/scene.hpp>
#include <levk/scene/skeleton_controller.hpp>
#include <levk/scene/skinned_mesh_renderer.hpp>
#include <levk/scene/static_mesh_renderer.hpp>
#include <levk/util/bool.hpp>
#include <levk/util/enumerate.hpp>
#include <levk/util/error.hpp>
#include <levk/util/logger.hpp>
#include <levk/util/visitor.hpp>
#include <levk/util/zip_ranges.hpp>
#include <filesystem>
#include <fstream>
#include <ranges>

namespace legsmi {
namespace fs = std::filesystem;
namespace asset = levk::asset;

template <typename T>
using Ptr = levk::Ptr<T>;

namespace {
constexpr levk::AlphaMode from(gltf2cpp::AlphaMode const mode) {
	switch (mode) {
	default:
	case gltf2cpp::AlphaMode::eOpaque: return levk::AlphaMode::eOpaque;
	case gltf2cpp::AlphaMode::eBlend: return levk::AlphaMode::eBlend;
	case gltf2cpp::AlphaMode::eMask: return levk::AlphaMode::eMask;
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

levk::Transform from(glm::mat4 const& mat) {
	auto ret = levk::Transform{};
	glm::vec3 scale{1.0f}, pos{}, skew{};
	glm::vec4 persp{};
	glm::quat orn{levk::quat_identity_v};
	glm::decompose(mat, scale, orn, pos, skew, persp);
	ret.set_position(pos);
	ret.set_orientation(orn);
	ret.set_scale(scale);
	return ret;
}

levk::Transform from(gltf2cpp::Transform const& transform) {
	auto ret = levk::Transform{};
	auto visitor = levk::Visitor{
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

levk::Camera from(gltf2cpp::Camera const& camera) {
	auto ret = levk::Camera{};
	ret.name = camera.name;
	auto const visitor = levk::Visitor{
		[&](gltf2cpp::Camera::Orthographic const& o) {
			auto out = levk::Camera::Orthographic{};
			out.view_plane.near = o.znear;
			out.view_plane.far = o.zfar;
			ret.type = out;
		},
		[&](gltf2cpp::Camera::Perspective const& p) {
			auto out = levk::Camera::Perspective{};
			out.view_plane.near = p.znear;
			if (p.zfar) { out.view_plane.far = *p.zfar; }
			out.field_of_view = p.yfov;
			ret.type = out;
		},
	};
	std::visit(visitor, camera.payload);
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
levk::Interpolator<T> make_interpolator(std::span<float const> times, std::span<T const> values, gltf2cpp::Interpolation interpolation) {
	assert(times.size() == values.size());
	auto ret = levk::Interpolator<T>{};
	for (auto [t, v] : levk::zip_ranges(times, values)) { ret.keyframes.push_back({v, levk::Duration{t}}); }
	ret.interpolation = static_cast<levk::Interpolation>(interpolation);
	return ret;
}

levk::Geometry::Packed to_geometry(gltf2cpp::Mesh::Primitive const& primitive) {
	auto ret = levk::Geometry::Packed{};
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

// Joints in a GLTF skin reference nodes in the GLTF global tree via their indices.
// To construct a skinned mesh skeleton and its animations, the tree needs to be trimmed to
// only the nodes relevant to the skin, and node indices correspondingly retargeted.
// A skeleton instance will then add one node per joint to the engine's scene tree, copy
// the animations and rewrite each animator target to match its corresponding Id<Node>.
// The order of joints after all these transformations must be identical to the source data,
// hence the [Index => *] unordered_map invasion.
using GltfNodeToJoint = std::unordered_map<gltf2cpp::Index<gltf2cpp::Node>, levk::Id<levk::Node>>;

struct MapGltfNodesToJoints {
	gltf2cpp::Root const& root;

	std::vector<levk::Id<levk::Node>> ordered_joints{};
	levk::NodeTree joints{};
	GltfNodeToJoint mapping{};

	void add_node(std::size_t index) {
		auto const& in_node = root.nodes[index];
		auto& out_node = joints.add(levk::Node::CreateInfo{
			.transform = from(in_node.transform),
			.name = in_node.name,
		});
		mapping.insert_or_assign(index, out_node.id());
	}

	void add_node_and_children(std::size_t index) {
		auto const& in_node = root.nodes[index];
		add_node(index);
		for (auto const child_index : in_node.children) { add_node_and_children(child_index); }
	}

	void setup_hierarchy() {
		for (auto const& [in_index, node_id] : mapping) {
			auto* out_node = joints.find(node_id);
			assert(out_node);
			auto const& in_node = root.nodes[in_index];
			if (in_node.parent) {
				auto it = mapping.find(*in_node.parent);
				assert(it != mapping.end());
				joints.reparent(*out_node, it->second);
			}
		}
	}

	std::size_t find_root_of(std::size_t node_index) const {
		auto const& node = root.nodes[node_index];
		if (!node.parent) { return node_index; }
		return find_root_of(*node.parent);
	}

	void setup_for(gltf2cpp::Skin const& skin) {
		if (skin.skeleton) { add_node(*skin.skeleton); }
		for (auto index : skin.joints) {
			index = find_root_of(index);
			if (!mapping.contains(index)) { add_node_and_children(index); }
		}
		for (auto const index : skin.joints) {
			auto const it = mapping.find(index);
			assert(it != mapping.end());
			ordered_joints.push_back(it->second);
		}
		setup_hierarchy();
	}
};

struct Resource {
	struct {
		std::string in{};
		std::string out{};
		std::string log{};
	} name{};
	std::size_t index{};
};

std::vector<Mesh> make_gltf_mesh_list(dj::Json const& json, bool& has_skinned_mesh) {
	auto ret = std::vector<Mesh>{};
	for (auto const [mesh, index] : levk::enumerate(json["meshes"].array_view())) {
		auto const& primitives = mesh["primitives"];
		if (!primitives || primitives.array_view().empty()) { continue; }
		auto const& attributes = primitives[0]["attributes"];
		if (attributes.object_view().empty()) { continue; }
		auto mesh_type = levk::MeshType::eStatic;
		if (attributes.contains("JOINTS_0")) {
			mesh_type = levk::MeshType::eSkinned;
			has_skinned_mesh = true;
		}
		ret.push_back(Mesh{mesh["name"].as<std::string>(), mesh_type, index});
	}
	return ret;
}

std::vector<Scene> make_gltf_scene_list(dj::Json const& json) {
	auto ret = std::vector<Scene>{};
	for (auto const [scene, index] : levk::enumerate(json["scenes"].array_view())) { ret.push_back(Scene{scene["name"].as<std::string>(), index}); }
	return ret;
}

std::string log_name(std::string_view in, std::size_t index) {
	auto ret = fmt::format("{}", index);
	if (!in.empty()) { fmt::format_to(std::back_inserter(ret), " - {}", in); }
	return ret;
}

std::string asset_name(std::string_view in, std::string_view type, std::size_t index) {
	auto ret = std::string{};
	ret.reserve(128);
	if (!in.empty() && in != "(Unnamed)") { fmt::format_to(std::back_inserter(ret), "{}.", in); }
	fmt::format_to(std::back_inserter(ret), "{}_{}", type, index);
	return ret;
}

Resource make_resource(std::string_view in, std::string_view type, std::size_t index) {
	auto ret = Resource{.index = index};
	ret.name.in = in;
	ret.name.out = asset_name(in, type, index);
	ret.name.log = log_name(in, index);
	return ret;
}

struct Exporter {
	template <typename T>
	using Index = gltf2cpp::Index<T>;

	ImportMap& out_imported;
	Logger const& import_logger;
	gltf2cpp::Root const& in_root;
	fs::path in_dir;
	fs::path uri_prefix;
	fs::path dir_uri;
	bool overwrite;

	std::optional<Index<gltf2cpp::Skin>> find_skin(Resource const& resource) const {
		for (auto const [node, index] : levk::enumerate(in_root.nodes)) {
			if (node.mesh && *node.mesh == resource.index) {
				if (!node.skin) {
					import_logger.error("[legsmi] GLTF node [{}] with SkinnedMesh [{}] does not have a GLTF Skin", index, resource.name.log);
					return {};
				}
				return *node.skin;
			}
		}
		import_logger.error("[legsmi] No Skin found for SkinnedMesh [{}]", resource.name.log);
		return {};
	}

	bool should_overwrite(std::string const& uri) const {
		if (out_imported.all.contains(uri)) { return false; }
		if (!fs::exists(uri)) { return true; }
		if (overwrite) {
			import_logger.info("[legsmi] Overwriting target asset: [{}]", uri);
			fs::remove(uri);
			return true;
		} else {
			import_logger.info("[legsmi] Import target exists, reusing: [{}]", uri);
		}
		return false;
	}

	std::string copy_image(gltf2cpp::Image const& in, std::size_t index, std::string_view subdir = "textures") {
		if (auto it = out_imported.images.find(index); it != out_imported.images.end()) { return it->second; }
		auto filename = in.source_filename.empty() ? fmt::format("image_{}.bin", index) : in.source_filename;
		auto uri = (dir_uri / subdir / filename).generic_string();
		auto dst = uri_prefix / uri;
		if (!should_overwrite(dst.generic_string())) { return uri; }
		fs::create_directories(dst.parent_path());
		if (in.source_filename.empty()) {
			assert(!in.bytes.empty());
			auto file = std::ofstream{dst, std::ios::binary};
			assert(file);
			file.write(reinterpret_cast<char const*>(in.bytes.data()), static_cast<std::streamsize>(in.bytes.size()));
		} else {
			fs::copy_file(in_dir / in.source_filename, dst);
		}
		import_logger.info("[legsmi] Image [{}] copied", uri);
		out_imported.add_to(out_imported.images, index, uri);
		return uri;
	}

	std::string copy_image(gltf2cpp::Texture const& in, std::size_t index, std::string_view subdir = "textures") {
		return copy_image(in_root.images[in.source], index, subdir);
	}

	levk::Uri<levk::Texture> export_texture(gltf2cpp::Texture const& in, std::size_t index, levk::ColourSpace colour_space) {
		auto image_uri = copy_image(in_root.images[in.source], index);
		auto json_uri = fs::path{image_uri};
		json_uri = json_uri.parent_path() / json_uri.stem();
		auto uri = fmt::format("{}.json", json_uri.generic_string());
		auto dst = uri_prefix / uri;
		if (!should_overwrite(dst.generic_string())) { return uri; }

		auto json = dj::Json{};
		json["image"] = std::move(image_uri);
		json["colour_space"] = colour_space == levk::ColourSpace::eLinear ? "linear" : "sRGB";
		json.to_file(dst.string().c_str());
		import_logger.info("[legsmi] Texture [{}] imported", uri);
		return uri;
	}

	levk::Uri<levk::Material> export_material(Resource const& resource, bool skinned) {
		if (auto it = out_imported.materials.find(resource.index); it != out_imported.materials.end()) { return it->second; }
		auto uri = (dir_uri / "materials" / fmt::format("{}.json", resource.name.out)).generic_string();
		auto dst = uri_prefix / uri;
		if (!should_overwrite(dst.generic_string())) { return uri; }

		fs::create_directories(dst.parent_path());

		auto const& in = in_root.materials[resource.index];
		auto material = asset::Material{};
		material.albedo = levk::Rgba::from(glm::vec4{in.pbr.base_color_factor[0], in.pbr.base_color_factor[1], in.pbr.base_color_factor[2], 1.0f});
		material.metallic = in.pbr.metallic_factor;
		material.roughness = in.pbr.roughness_factor;
		material.alpha_mode = from(in.alpha_mode);
		material.alpha_cutoff = in.alpha_cutoff;
		material.name = in.name;
		auto& textures = material.textures.uris;
		if (auto i = in.pbr.base_color_texture) { textures[0] = {export_texture(in_root.textures[i->texture], i->texture, levk::ColourSpace::eSrgb)}; }
		if (auto i = in.pbr.metallic_roughness_texture) {
			textures[1] = {export_texture(in_root.textures[i->texture], i->texture, levk::ColourSpace::eLinear)};
		}
		if (auto i = in.emissive_texture) { textures[2] = {export_texture(in_root.textures[i->texture], i->texture, levk::ColourSpace::eSrgb)}; }
		material.emissive_factor = {in.emissive_factor[0], in.emissive_factor[1], in.emissive_factor[2]};
		material.vertex_shader = skinned ? "shaders/skinned.vert" : "shaders/lit.vert";
		material.fragment_shader = "shaders/lit.frag";
		auto json = dj::Json{};
		to_json(json, material);
		if (!json.to_file(dst.string().c_str())) {
			import_logger.error("[legsmi] Failed to write material JSON [{}]", uri);
			return {};
		}
		import_logger.info("[legsmi] Material [{}] imported", uri);
		out_imported.add_to(out_imported.materials, resource.index, uri);
		return uri;
	}

	levk::Uri<asset::BinGeometry> export_geometry(gltf2cpp::Mesh const& in, std::size_t primitive_index, std::size_t mesh_index, std::vector<glm::uvec4> joints,
												  std::vector<glm::vec4> weights) {
		auto uri = (dir_uri / "geometries" / fmt::format("mesh_{}.geometry_{}.bin", mesh_index, primitive_index)).generic_string();
		auto dst = uri_prefix / uri;
		if (!should_overwrite(dst.generic_string())) { return uri; }

		fs::create_directories(dst.parent_path());

		auto bin = asset::BinGeometry{};
		bin.geometry = to_geometry(in.primitives[primitive_index]);
		bin.joints = std::move(joints);
		bin.weights = std::move(weights);
		[[maybe_unused]] bool const res = bin.write(dst.string().c_str());
		assert(res);
		import_logger.info("[legsmi] BinGeometry [{}] imported", uri);
		return uri;
	}

	levk::Uri<levk::Mesh> operator()(Resource const& resource) {
		auto uri = (dir_uri / fmt::format("{}.json", resource.name.out)).generic_string();
		auto dst = uri_prefix / uri;
		if (!should_overwrite(dst.generic_string())) { return uri; }

		fs::create_directories(dst.parent_path());

		auto out_mesh = asset::Mesh3D{.type = asset::Mesh3D::Type::eStatic};
		auto const& in_mesh = in_root.meshes[resource.index];
		for (auto const& [in_primitive, primitive_index] : levk::enumerate(in_mesh.primitives)) {
			bool const has_joints = !in_primitive.geometry.joints.empty();
			auto out_primitive = asset::Mesh3D::Primitive{};
			if (in_primitive.material) {
				auto const& in_material = in_root.materials[*in_primitive.material];
				out_primitive.material = export_material(make_resource(in_material.name, "material", *in_primitive.material), has_joints);
			}
			if (has_joints) { out_mesh.type = asset::Mesh3D::Type::eSkinned; }
			auto joints = std::vector<glm::uvec4>{};
			auto weights = std::vector<glm::vec4>{};
			if (has_joints) {
				joints.resize(in_primitive.geometry.joints[0].size());
				std::memcpy(joints.data(), in_primitive.geometry.joints[0].data(), std::span{in_primitive.geometry.joints[0]}.size_bytes());
				weights.resize(in_primitive.geometry.weights[0].size());
				std::memcpy(weights.data(), in_primitive.geometry.weights[0].data(), std::span{in_primitive.geometry.weights[0]}.size_bytes());
			}
			out_primitive.geometry = export_geometry(in_mesh, primitive_index, resource.index, std::move(joints), std::move(weights));
			out_mesh.primitives.push_back(std::move(out_primitive));
		}
		if (out_mesh.primitives.empty()) {
			import_logger.warn("[legsmi] Mesh [{}] has no primitives, skipping", resource.name.log);
			return {};
		}
		if (out_mesh.type == asset::Mesh3D::Type::eSkinned) {
			auto const skin = find_skin(resource);
			if (!skin) {
				import_logger.error("[legsmi] Failed to locate GLTF Skin for SkinnedMesh [{}]", resource.name.log);
				return {};
			}
			if (*skin >= in_root.skins.size()) {
				import_logger.error("[legsmi] SkinnedMesh [{}] has an invalid a GLTF Skin index: [{}]", resource.name.log, *skin);
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
		json.to_file(dst.string().c_str());
		import_logger.info("[legsmi] Mesh [{}] imported", uri);
		out_imported.add_to(out_imported.meshes, resource.index, uri);
		return uri;
	}

	levk::Uri<asset::BinSkeletalAnimation> export_skeletal_animation(Resource const& resource, asset::BinSkeletalAnimation const& animation) {
		auto uri = (dir_uri / "animations" / fmt::format("{}.bin", resource.name.out)).generic_string();
		auto dst = uri_prefix / uri;
		if (!should_overwrite(dst.generic_string())) { return uri; }

		fs::create_directories(dst.parent_path());

		if (!animation.write(dst.generic_string().c_str())) {
			import_logger.error("[legsmi] Failed to import Skeletal Animation: [{}]", uri);
			return {};
		}
		import_logger.info("[legsmi] Skeleton Animation [{}] imported", uri);
		out_imported.add_to(out_imported.animations, resource.index, uri);
		return uri;
	}

	asset::BinSkeletalAnimation to_skeletal_animation(GltfNodeToJoint const& mapping, std::size_t index) {
		auto const& in_animation = in_root.animations[index];
		auto const resource = make_resource(in_animation.name, "animation", index);
		auto ret = asset::BinSkeletalAnimation{};
		ret.name = in_animation.name;
		for (auto const& in_channel : in_animation.channels) {
			if (!in_channel.target.node) { continue; }
			auto joint_it = mapping.find(*in_channel.target.node);
			if (joint_it == mapping.end()) { continue; }
			using Path = gltf2cpp::Animation::Path;
			auto out_sampler = asset::BinSkeletalAnimation::Sampler{};
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
					out_sampler = levk::TransformAnimation::Scale{make_interpolator<glm::vec3>(times, vec, in_sampler.interpolation)};
				} else {
					out_sampler = levk::TransformAnimation::Translate{make_interpolator<glm::vec3>(times, vec, in_sampler.interpolation)};
				}
				break;
			}
			case Path::eRotation: {
				assert(output.type == gltf2cpp::Accessor::Type::eVec4);
				auto vec = std::vector<glm::quat>{};
				vec.resize(values.size() / 4);
				std::memcpy(vec.data(), values.data(), values.size_bytes());
				out_sampler = levk::TransformAnimation::Rotate{make_interpolator<glm::quat>(times, vec, in_sampler.interpolation)};
				break;
			}
			default:
				// TODO: not implemented
				continue;
			}

			ret.samplers.push_back(std::move(out_sampler));
			ret.target_joints.push_back(joint_it->second);
		}
		if (!ret.samplers.empty()) {
			assert(ret.samplers.size() == ret.target_joints.size());
			export_skeletal_animation(resource, ret);
		}
		return ret;
	}

	levk::Uri<levk::Skeleton> export_skeleton(Resource const& resource_skin) {
		auto uri = (dir_uri / "skeletons" / fmt::format("{}.json", resource_skin.name.out)).generic_string();
		auto dst = uri_prefix / uri;
		if (!should_overwrite(dst.generic_string())) { return uri; }

		fs::create_directories(dst.parent_path());

		auto skin_node = Ptr<gltf2cpp::Node const>{};
		for (auto& node : in_root.nodes) {
			if (node.skin && *node.skin == resource_skin.index) { skin_node = &node; }
		}
		if (!skin_node) { return {}; }

		assert(skin_node->mesh.has_value());
		if (auto it = out_imported.skeletons.find(resource_skin.index); it != out_imported.skeletons.end()) { return it->second; }

		auto const& in_skin = in_root.skins[resource_skin.index];
		auto mapper = MapGltfNodesToJoints{in_root};
		mapper.setup_for(in_skin);
		auto asset = levk::Skeleton{.joint_tree = std::move(mapper.joints), .ordered_joints_ids = std::move(mapper.ordered_joints)};

		for (auto const [in_animation, index] : levk::enumerate(in_root.animations)) {
			auto const animation_prefix = make_resource(in_animation.name, "animation", index);
			auto const skeletal_animation = to_skeletal_animation(mapper.mapping, index);
			asset.animations.push_back(export_skeletal_animation(animation_prefix, skeletal_animation));
		}

		asset.name = fs::path{resource_skin.name.out}.stem().string();
		auto json = dj::Json{};
		asset::to_json(json, asset);
		if (!json.to_file(dst.string().c_str())) {
			import_logger.error("[legsmi] Failed to write Skeleton JSON [{}]", uri);
			return {};
		}
		import_logger.info("[legsmi] Skeleton [{}] imported", uri);
		out_imported.add_to(out_imported.skeletons, resource_skin.index, uri);
		return uri;
	}
};

struct SceneWalker {
	ImportMap& out_imported;
	MeshImporter const& importer;
	AssetList const& asset_list;
	std::string export_uri;

	levk::Level& out_level;

	std::unordered_map<std::size_t, levk::Uri<levk::StaticMesh>> imported_meshes{};

	Mesh const& get_mesh(std::size_t index) const {
		auto func = [index](Mesh const& in) { return in.index == index; };
		auto it = std::ranges::find_if(asset_list.meshes, func);
		assert(it != asset_list.meshes.end());
		return *it;
	}

	levk::Uri<> const& import_mesh(std::size_t index) {
		if (auto it = imported_meshes.find(index); it != imported_meshes.end()) { return it->second; }
		auto uri = importer.try_import(get_mesh(index), out_imported);
		assert(!uri.value().empty());
		auto [it, _] = imported_meshes.insert_or_assign(index, std::move(uri));
		return it->second;
	}

	void add_node(std::size_t index, levk::Id<levk::Node> parent) {
		auto const& in_node = importer.root.nodes[index];
		auto& out_node = out_level.node_tree.add(levk::NodeCreateInfo{.transform = legsmi::from(in_node.transform), .name = in_node.name, .parent = parent});
		if (in_node.mesh) {
			auto attachment = levk::MeshAttachment{};
			attachment.uri = import_mesh(*in_node.mesh);
			auto json = importer.serializer->serialize(attachment);
			if (json) { out_level.attachments_map[out_node.id()].push_back(std::move(json)); }
		}
		// TODO: cameras
		if (in_node.camera) { out_level.camera = from(importer.root.cameras[*in_node.camera]); }
		parent = out_node.id();
		for (auto const node_index : in_node.children) { add_node(node_index, parent); }
	}

	std::vector<levk::Uri<levk::StaticMesh>> operator()(Scene const& scene) {
		auto const& in_scene = importer.root.scenes[scene.index];
		for (auto const node_index : in_scene.root_nodes) { add_node(node_index, {}); }
		auto ret = std::vector<levk::Uri<levk::StaticMesh>>{};
		ret.reserve(imported_meshes.size());
		for (auto& [_, uri] : imported_meshes) { ret.push_back(std::move(uri)); }
		return ret;
	}
};
} // namespace

std::string AssetList::make_default_level_uri(std::size_t scene_index) const {
	auto uri = fs::path{gltf_path}.stem();
	if (scene_index < scenes.size() && !scenes[scene_index].name.empty()) {
		uri /= scenes[scene_index].name;
	} else {
		uri /= uri.filename();
	}
	return fmt::format("{}.level_{}.json", uri.generic_string(), scene_index);
}

MeshImporter AssetList::mesh_importer(std::string root_path, std::string dir_uri, Logger import_logger, bool overwrite) const {
	auto ret = MeshImporter{.import_logger = std::move(import_logger), .serializer = serializer, .overwrite_existing = overwrite};
	if (gltf_path.empty()) {
		ret.import_logger.error("[legsmi] Empty GLTF file path");
		return ret;
	}
	if (root_path.empty()) {
		ret.import_logger.error("[legsmi] Empty root path / prefix");
		return ret;
	}
	ret.root = gltf2cpp::parse(gltf_path.c_str());
	if (!ret.root) {
		ret.import_logger.error("[legsmi] Failed to parse GLTF [{}]", gltf_path);
		return ret;
	}
	ret.src_dir = fs::path{gltf_path}.parent_path().string();
	ret.uri_prefix = root_path;
	ret.dir_uri = dir_uri;
	return ret;
}

SceneImporter AssetList::scene_importer(std::string root_path, std::string dir_uri, std::string scene_uri, Logger import_logger, bool overwrite) const {
	auto ret = SceneImporter{};
	ret.mesh_importer = mesh_importer(root_path, dir_uri, std::move(import_logger), overwrite);
	if (!ret.mesh_importer) { return {}; }
	ret.asset_list = peek_assets(gltf_path, serializer);
	ret.level_uri = std::move(scene_uri);
	return ret;
}

levk::Uri<levk::Mesh> MeshImporter::try_import(Mesh const& mesh, ImportMap& out_imported) const {
	auto const dst_dir = fs::path{uri_prefix} / dir_uri;
	try {
		if (!fs::exists(dst_dir)) { fs::create_directories(dst_dir); }
		return Exporter{
			.out_imported = out_imported,
			.import_logger = import_logger,
			.in_root = root,
			.in_dir = src_dir,
			.uri_prefix = uri_prefix,
			.dir_uri = dir_uri,
			.overwrite = overwrite_existing,
		}(make_resource(mesh.name, "mesh", mesh.index));
	} catch (std::exception const& e) {
		import_logger.error("[legsmi] Fatal error: {}", e.what());
		return {};
	}
}

levk::Uri<levk::Level> SceneImporter::try_import(Scene const& scene, ImportMap& out_imported) const {
	auto const export_path = fs::path{mesh_importer.uri_prefix} / mesh_importer.dir_uri;
	try {
		auto out_level = levk::Level{};
		auto ret = SceneWalker{out_imported, mesh_importer, asset_list, export_path.generic_string(), out_level}(scene);
		out_level.name = fs::path{level_uri}.stem().stem().string();

		auto scene_path = fs::path{mesh_importer.uri_prefix} / level_uri;
		auto json = dj::Json{};
		asset::to_json(json, out_level);
		json.to_file(scene_path.string().c_str());

		return level_uri;
	} catch (std::exception const& e) {
		mesh_importer.import_logger.error("[legsmi] Fatal error: {}", e.what());
		return {};
	}
}
} // namespace legsmi

levk::Transform legsmi::from(gltf2cpp::Transform const& transform) {
	auto ret = levk::Transform{};
	auto visitor = levk::Visitor{
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

auto legsmi::peek_assets(std::string gltf_path, levk::NotNull<levk::Serializer const*> serializer, Logger const& import_logger) -> AssetList {
	auto ret = AssetList{.serializer = serializer};
	if (!fs::is_regular_file(gltf_path)) {
		import_logger.error("[legsmi] Invalid GLTF file path [{}]", gltf_path);
		return ret;
	}
	auto json = dj::Json::from_file(gltf_path.c_str());
	auto uri = fs::path{gltf_path}.stem();
	ret.default_dir_uri = uri.string() + "/";
	ret.gltf_path = std::move(gltf_path);
	ret.meshes = make_gltf_mesh_list(json, ret.has_skinned_mesh);
	ret.scenes = make_gltf_scene_list(json);
	return ret;
}
