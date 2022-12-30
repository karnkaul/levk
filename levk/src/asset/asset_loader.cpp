#include <levk/asset/asset_loader.hpp>
#include <levk/asset/common.hpp>
#include <filesystem>

namespace levk {
namespace fs = std::filesystem;

Id<Texture> AssetLoader::load_texture(char const* path, ColourSpace colour_space) const {
	auto image = Image{path, fs::path{path}.filename().string()};
	if (!image) {
		import_logger.error("[Load] Failed to load image [{}]", path);
		return {};
	}
	auto const tci = TextureCreateInfo{
		.name = fs::path{path}.filename().string(),
		.mip_mapped = colour_space == ColourSpace::eSrgb,
		.colour_space = colour_space,
	};
	auto texture = graphics_device.make_texture(image, std::move(tci));
	import_logger.info("[Load] [{}] Texture loaded", texture.name());
	return mesh_resources.textures.add(std::move(texture)).first;
}

Id<StaticMesh> AssetLoader::try_load_static_mesh(char const* path) const {
	auto json = dj::Json::from_file(path);
	if (!json) {
		import_logger.error("[Load] Failed to open [{}]", path);
		return {};
	}
	if (json["asset_type"].as_string() != "mesh") {
		import_logger.error("[Load] JSON is not a Mesh [{}]", path);
		return {};
	}
	if (json["type"].as_string() != "static") { return {}; }
	return load_static_mesh(path, json);
}

Id<StaticMesh> AssetLoader::load_static_mesh(char const* path, dj::Json const& json) const {
	if (json["type"].as_string() != "static") {
		import_logger.error("[Load] JSON is not a StaticMesh [{}]", path);
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
		auto bin_geometry = asset::BinGeometry{};
		if (!bin_geometry.read((dir / in_geometry).string().c_str())) {
			import_logger.error("[Load] Failed to read bin geometry [{}]", in_geometry);
			continue;
		}
		auto asset_material = asset::Material{};
		if (!in_material.empty()) {
			auto json = dj::Json::from_file((dir / in_material).string().c_str());
			if (!json) {
				import_logger.error("[Load] Failed to open material JSON [{}]", in_material);
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
		if (!asset_material.base_colour.empty()) { material.base_colour = load_texture(asset_path(asset_material.base_colour).c_str(), ColourSpace::eSrgb); }
		if (!asset_material.emissive.empty()) { material.emissive = load_texture(asset_path(asset_material.emissive).c_str(), ColourSpace::eSrgb); }
		if (!asset_material.roughness_metallic.empty()) {
			material.roughness_metallic = load_texture(asset_path(asset_material.roughness_metallic).c_str(), ColourSpace::eLinear);
		}
		auto material_id = mesh_resources.materials.add(std::move(material)).first;
		auto geometry = graphics_device.make_mesh_geometry(bin_geometry.geometry, {bin_geometry.joints, bin_geometry.weights});
		mesh.primitives.push_back(MeshPrimitive{std::move(geometry), material_id});
	}
	import_logger.info("[Load] [{}] StaticMesh loaded", mesh.name);
	return mesh_resources.static_meshes.add(std::move(mesh)).first;
}

Id<levk::Skeleton> AssetLoader::load_skeleton(char const* path) const {
	auto json = dj::Json::from_file(path);
	if (!json) {
		import_logger.error("[Load] Failed to open [{}]", path);
		return {};
	}
	if (json["asset_type"].as_string() != "skeleton") {
		import_logger.error("[Load] JSON is not a Skeleton [{}]", path);
		return {};
	}
	auto asset = asset::Skeleton{};
	asset::from_json(json, asset);
	import_logger.info("[Load] [{}] Skeleton loaded", asset.skeleton.name);
	return mesh_resources.skeletons.add(std::move(asset.skeleton)).first;
}

Id<SkinnedMesh> AssetLoader::try_load_skinned_mesh(char const* path) const {
	auto json = dj::Json::from_file(path);
	if (!json) {
		import_logger.error("[Load] Failed to open [{}]", path);
		return {};
	}
	if (json["asset_type"].as_string() != "mesh") {
		import_logger.error("[Load] JSON is not a Mesh [{}]", path);
		return {};
	}
	if (json["type"].as_string() != "skinned") { return {}; }
	return load_skinned_mesh(path, json);
}

Id<SkinnedMesh> AssetLoader::load_skinned_mesh(char const* path, dj::Json const& json) const {
	if (json["type"].as_string() != "skinned") {
		import_logger.error("[Load] JSON is not a SkinnedMesh [{}]", path);
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
		auto bin_geometry = asset::BinGeometry{};
		if (!bin_geometry.read((dir / in_geometry).string().c_str())) {
			import_logger.error("[Load] Failed to read bin geometry [{}]", in_geometry);
			continue;
		}
		auto asset_material = asset::Material{};
		if (!in_material.empty()) {
			auto json = dj::Json::from_file((dir / in_material).string().c_str());
			if (!json) {
				import_logger.error("[Load] Failed to open material JSON [{}]", in_material);
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
		if (!asset_material.base_colour.empty()) { material.base_colour = load_texture(asset_path(asset_material.base_colour).c_str(), ColourSpace::eSrgb); }
		if (!asset_material.emissive.empty()) { material.emissive = load_texture(asset_path(asset_material.emissive).c_str(), ColourSpace::eSrgb); }
		if (!asset_material.roughness_metallic.empty()) {
			material.roughness_metallic = load_texture(asset_path(asset_material.roughness_metallic).c_str(), ColourSpace::eLinear);
		}
		auto material_id = mesh_resources.materials.add(std::move(material)).first;
		auto geometry = graphics_device.make_mesh_geometry(bin_geometry.geometry, {bin_geometry.joints, bin_geometry.weights});
		mesh.primitives.push_back(MeshPrimitive{std::move(geometry), material_id});
	}

	if (auto const& skeleton = json["skeleton"]) {
		auto const skeleton_uri = dir / skeleton.as_string();
		mesh.skeleton = load_skeleton(skeleton_uri.c_str());
	}
	import_logger.info("[Load] [{}] SkinnedMesh loaded", mesh.name);
	return mesh_resources.skinned_meshes.add(std::move(mesh)).first;
}

std::variant<std::monostate, Id<StaticMesh>, Id<SkinnedMesh>> AssetLoader::try_load_mesh(char const* path) const {
	if (auto ret = try_load_skinned_mesh(path)) { return ret; }
	if (auto ret = try_load_static_mesh(path)) { return ret; }
	import_logger.error("[Load] Failed to load mesh [{}]", path);
	return std::monostate{};
}
} // namespace levk
