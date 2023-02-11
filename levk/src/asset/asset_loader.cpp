#include <levk/asset/asset_loader.hpp>
#include <levk/asset/common.hpp>
#include <levk/serializer.hpp>
#include <levk/service.hpp>
#include <levk/util/logger.hpp>
#include <filesystem>

namespace levk {
namespace {
namespace fs = std::filesystem;

std::unique_ptr<MaterialBase> to_material_base(Uri<> const& parent, dj::Json const& json) {
	auto ret = Service<Serializer>::locate().deserialize_as<MaterialBase>(json);
	if (!ret) { return {}; }
	for (auto& info : ret.value->textures.textures) {
		if (!info.uri) { continue; }
		info.uri = parent.append(info.uri.value());
	}
	return std::move(ret.value);
}
} // namespace

std::span<std::byte const> AssetLoader::load_bytes(Reader& reader, Uri<> const& uri, std::uint8_t reader_flags) {
	return reader.load(uri.value(), reader_flags).bytes;
}

dj::Json AssetLoader::load_json(Reader& reader, Uri<> const& uri, std::uint8_t reader_flags) {
	auto bytes = load_bytes(reader, uri, reader_flags);
	if (bytes.empty()) { return {}; }
	return dj::Json::parse(std::string_view{reinterpret_cast<char const*>(bytes.data()), bytes.size()});
}

std::string AssetLoader::get_asset_type(Reader& reader, Uri<> const& uri, std::uint8_t reader_flags) {
	auto json = load_json(reader, uri, reader_flags);
	if (!json) { return {}; }
	return json["asset_type"].as<std::string>();
}

MeshType AssetLoader::get_mesh_type(Reader& reader, Uri<> const& uri, std::uint8_t reader_flags) {
	auto json = load_json(reader, uri, reader_flags);
	if (!json || json["asset_type"].as_string() != "mesh") { return MeshType::eNone; }
	auto const type = json["type"].as_string();
	if (type == "skinned") { return MeshType::eSkinned; }
	if (type == "static") { return MeshType::eStatic; }
	return MeshType::eNone;
}

Ptr<Texture> AssetLoader::load_texture(Uri<> const& uri, ColourSpace colour_space) const {
	if (auto ret = render_resources.textures.find(uri)) { return ret; }
	auto bytes = load_bytes(reader, uri);
	if (bytes.empty()) { return {}; }
	auto image = Image{bytes, fs::path{uri.value()}.filename().string()};
	if (!image) {
		logger::error("[Load] Failed to load image [{}]", uri.value());
		return {};
	}
	auto const tci = TextureCreateInfo{
		.name = std::string{image.name()},
		.mip_mapped = colour_space == ColourSpace::eSrgb,
		.colour_space = colour_space,
	};
	auto texture = graphics_device.make_texture(image, std::move(tci));
	logger::info("[Load] [{}] Texture loaded", uri.value());
	return &render_resources.textures.add(uri, std::move(texture));
}

Ptr<Material> AssetLoader::load_material(Uri<> const& uri) const {
	if (auto ret = render_resources.materials.find(uri)) { return ret; }
	auto json = load_json(reader, uri);
	if (!json) { return {}; }
	auto mat = to_material_base(uri.parent(), json);
	if (!mat) {
		logger::error("[Load] Failed to load Material [{}]", uri.value());
		return {};
	}
	for (auto const& [tex_uri, colour_space] : mat->textures.textures) {
		if (tex_uri) { load_texture(tex_uri, colour_space); }
	}
	logger::info("[Load] [{}] Material loaded", uri.value());
	return &render_resources.materials.add(uri, Material{std::move(mat)});
}

Ptr<StaticMesh> AssetLoader::load_static_mesh(Uri<> const& uri) const {
	if (auto ret = render_resources.static_meshes.find(uri)) { return ret; }
	auto json = load_json(reader, uri);
	if (!json) { return {}; }
	if (json["type"].as_string() != "static") {
		logger::error("[Load] JSON is not a StaticMesh [{}]", uri.value());
		return {};
	}
	auto dir_uri = Uri{uri.parent()};
	auto mesh = StaticMesh{};
	mesh.name = json["name"].as_string();
	auto asset = asset::Mesh{};
	asset::from_json(json, asset);
	for (auto const& in_primitive : asset.primitives) {
		assert(!in_primitive.geometry.value().empty());
		auto bin_geometry = asset::BinGeometry{};
		auto bin_geometry_uri = Uri{dir_uri.append(in_primitive.geometry.value())};
		if (!bin_geometry.read(reader.load(bin_geometry_uri.value()).bytes)) {
			logger::error("[Load] Failed to read bin geometry [{}]", bin_geometry_uri.value());
			continue;
		}
		auto geometry = graphics_device.make_mesh_geometry(bin_geometry.geometry, {bin_geometry.joints, bin_geometry.weights});
		auto material_uri = Uri<Material>{};
		if (!in_primitive.material.value().empty()) {
			material_uri = Uri{dir_uri}.append(in_primitive.material.value());
			load_material(material_uri);
		}
		mesh.primitives.push_back(MeshPrimitive{std::move(geometry), std::move(material_uri)});
	}
	logger::info("[Load] [{}] StaticMesh loaded", uri.value());
	return &render_resources.static_meshes.add(uri, std::move(mesh));
}

Ptr<Skeleton> AssetLoader::load_skeleton(Uri<> const& uri) const {
	if (auto ret = render_resources.skeletons.find(uri)) { return ret; }
	auto const json = load_json(reader, uri);
	if (!json) { return {}; }
	if (json["asset_type"].as_string() != "skeleton") {
		logger::error("[Load] JSON is not a Skeleton [{}]", uri.value());
		return {};
	}
	auto asset = asset::Skeleton{};
	asset::from_json(json, asset);
	auto dir_uri = Uri{uri.parent()};
	auto skeleton = Skeleton{};
	skeleton.name = std::string{json["name"].as_string()};
	skeleton.joints = std::move(asset.joints);
	for (auto const& in_animation : asset.animations) {
		auto animation_uri = Uri{dir_uri.append(in_animation.value())};
		auto const in_animation_asset = load_json(reader, animation_uri);
		if (!in_animation_asset) { continue; }
		auto out_animation_asset = asset::BinSkeletalAnimation{};
		if (!out_animation_asset.read(reader.load(animation_uri.value()).bytes)) {
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
	skeleton.self = uri;
	logger::info("[Load] [{}] Skeleton loaded", asset.name);
	return &render_resources.skeletons.add(uri, std::move(skeleton));
}

Ptr<SkinnedMesh> AssetLoader::load_skinned_mesh(Uri<> const& uri) const {
	if (auto ret = render_resources.skinned_meshes.find(uri)) { return ret; }
	auto const json = load_json(reader, uri);
	if (json["type"].as_string() != "skinned") {
		logger::error("[Load] JSON is not a SkinnedMesh [{}]", uri.value());
		return {};
	}
	auto dir_uri = Uri{uri.parent()};
	auto mesh = SkinnedMesh{};
	mesh.name = json["name"].as_string();
	auto asset = asset::Mesh{};
	asset::from_json(json, asset);
	for (auto const& in_primitive : asset.primitives) {
		assert(!in_primitive.geometry.value().empty());
		auto bin_geometry = asset::BinGeometry{};
		auto bin_geometry_uri = Uri{dir_uri.append(in_primitive.geometry.value())};
		if (!bin_geometry.read(reader.load(bin_geometry_uri.value()).bytes)) {
			logger::error("[Load] Failed to read bin geometry [{}]", bin_geometry_uri.value());
			continue;
		}
		auto geometry = graphics_device.make_mesh_geometry(bin_geometry.geometry, {bin_geometry.joints, bin_geometry.weights});
		auto material_uri = Uri<Material>{};
		if (!in_primitive.material.value().empty()) {
			material_uri = Uri{dir_uri}.append(in_primitive.material.value());
			load_material(material_uri);
		}
		mesh.primitives.push_back(MeshPrimitive{std::move(geometry), std::move(material_uri)});
	}
	mesh.inverse_bind_matrices = asset.inverse_bind_matrices;
	if (auto const& skeleton = json["skeleton"]) {
		auto const skeleton_uri = dir_uri.append(skeleton.as_string());
		if (load_skeleton(skeleton_uri)) { mesh.skeleton = skeleton_uri; }
	}
	logger::info("[Load] [{}] SkinnedMesh loaded", mesh.name);
	return &render_resources.skinned_meshes.add(uri, std::move(mesh));
}
} // namespace levk
