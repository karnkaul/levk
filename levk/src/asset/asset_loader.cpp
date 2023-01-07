#include <levk/asset/asset_loader.hpp>
#include <levk/asset/common.hpp>
#include <levk/util/logger.hpp>
#include <filesystem>

namespace levk {
namespace {
namespace fs = std::filesystem;

std::string asset_path(fs::path const& dir, std::string_view const suffix) { return (dir / suffix).string(); };

template <typename T>
T make_mesh(dj::Json const& json, AssetLoader const& loader, fs::path const& dir) {
	auto ret = T{};
	ret.name = json["name"].as_string();
	auto asset = asset::Mesh{};
	asset::from_json(json, asset);
	for (auto const& in_primitive : asset.primitives) {
		assert(!in_primitive.geometry.value().empty());
		auto bin_geometry = asset::BinGeometry{};
		if (!bin_geometry.read(asset_path(dir, in_primitive.geometry.value()).c_str())) {
			logger::error("[Load] Failed to read bin geometry [{}]", in_primitive.geometry.value());
			continue;
		}
		auto asset_material = asset::Material{};
		if (!in_primitive.material.value().empty()) {
			auto json = dj::Json::from_file(asset_path(dir, in_primitive.material.value()).c_str());
			if (!json) {
				logger::error("[Load] Failed to open material JSON [{}]", in_primitive.material.value());
			} else {
				asset::from_json(json, asset_material);
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
		if (!asset_material.base_colour.empty()) {
			material.base_colour = loader.load_texture(asset_path(dir, asset_material.base_colour).c_str(), ColourSpace::eSrgb);
		}
		if (!asset_material.emissive.empty()) { material.emissive = loader.load_texture(asset_path(dir, asset_material.emissive).c_str(), ColourSpace::eSrgb); }
		if (!asset_material.roughness_metallic.empty()) {
			material.roughness_metallic = loader.load_texture(asset_path(dir, asset_material.roughness_metallic).c_str(), ColourSpace::eLinear);
		}
		auto material_id = loader.render_resources.materials.add(std::move(material)).first;
		auto geometry = loader.graphics_device.make_mesh_geometry(bin_geometry.geometry, {bin_geometry.joints, bin_geometry.weights});
		ret.primitives.push_back(MeshPrimitive{std::move(geometry), material_id});
	}
	if constexpr (std::same_as<T, SkinnedMesh>) { ret.inverse_bind_matrices = asset.inverse_bind_matrices; }
	return ret;
}

dj::Json open_json(char const* path) {
	auto json = dj::Json::from_file(path);
	if (!json) { logger::error("[Load] Failed to open [{}]", path); }
	return json;
}
} // namespace

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
	return render_resources.textures.add(std::move(texture)).first;
}

Id<StaticMesh> AssetLoader::try_load_static_mesh(char const* path) const {
	auto json = open_json(path);
	if (!json) { return {}; }
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
	auto mesh = make_mesh<StaticMesh>(json, *this, fs::path{path}.parent_path());
	logger::info("[Load] [{}] StaticMesh loaded", mesh.name);
	return render_resources.static_meshes.add(std::move(mesh)).first;
}

Id<levk::Skeleton> AssetLoader::load_skeleton(char const* path) const {
	auto const json = open_json(path);
	if (!json) { return {}; }
	if (json["asset_type"].as_string() != "skeleton") {
		logger::error("[Load] JSON is not a Skeleton [{}]", path);
		return {};
	}
	auto asset = asset::Skeleton{};
	asset::from_json(json, asset);
	auto const parent_dir = fs::path{path}.parent_path();
	auto skeleton = Skeleton{};
	skeleton.name = std::string{json["name"].as_string()};
	skeleton.joints = std::move(asset.joints);
	for (auto const& in_animation : asset.animations) {
		auto const in_animation_asset = open_json((parent_dir / in_animation.value()).generic_string().c_str());
		if (!in_animation_asset) {
			logger::warn("[Load] Failed to load Skeletal animation JSON [{}]", in_animation.value());
			continue;
		}
		auto out_animation_asset = asset::BinSkeletalAnimation{};
		if (!out_animation_asset.read((parent_dir / in_animation.value()).string().c_str())) {
			logger::warn("[Load] Failed to load SkeletalAnimation at: [{}]", in_animation.value());
			continue;
		}
		auto source = Skeleton::Animation::Source{
			.target_joints = std::move(out_animation_asset.target_joints),
			.name = std::move(out_animation_asset.name),
		};
		for (auto const& in_sampler : out_animation_asset.samplers) { source.animation.samplers.push_back({in_sampler}); }
		skeleton.animation_sources.push_back(std::move(source));
	}
	logger::info("[Load] [{}] Skeleton loaded", asset.name);
	return render_resources.skeletons.add(std::move(skeleton)).first;
}

Id<SkinnedMesh> AssetLoader::try_load_skinned_mesh(char const* path) const {
	auto const json = open_json(path);
	if (!json) { return {}; }
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
	auto const dir = fs::path{path}.parent_path();
	auto mesh = make_mesh<SkinnedMesh>(json, *this, dir);
	if (auto const& skeleton = json["skeleton"]) {
		auto const skeleton_uri = dir / skeleton.as_string();
		mesh.skeleton = load_skeleton(skeleton_uri.string().c_str());
	}
	logger::info("[Load] [{}] SkinnedMesh loaded", mesh.name);
	return render_resources.skinned_meshes.add(std::move(mesh)).first;
}

std::string AssetLoader::get_asset_type(char const* path) { return dj::Json::from_file(path)["asset_type"].as<std::string>(); }

MeshType AssetLoader::get_mesh_type(char const* path) {
	auto json = dj::Json::from_file(path);
	if (!json || json["asset_type"].as_string() != "mesh") { return MeshType::eNone; }
	auto const type = json["type"].as_string();
	if (type == "skinned") { return MeshType::eSkinned; }
	if (type == "static") { return MeshType::eStatic; }
	return MeshType::eNone;
}
} // namespace levk
