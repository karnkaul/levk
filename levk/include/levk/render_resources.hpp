#pragma once
#include <levk/material.hpp>
#include <levk/resource_map.hpp>
#include <levk/skeleton.hpp>
#include <levk/skinned_mesh.hpp>
#include <levk/static_mesh.hpp>

namespace levk {
struct RenderResources {
	ResourceMap<Texture> textures{};
	ResourceMap<Material> materials{};
	ResourceMap<StaticMesh> static_meshes{};
	ResourceMap<SkinnedMesh> skinned_meshes{};
	ResourceMap<Skeleton> skeletons{};

	Ptr<Skeleton const> find_skeleton(Uri<SkinnedMesh> const& mesh_uri) const {
		auto const* mesh = skinned_meshes.find(mesh_uri);
		if (!mesh) { return {}; }
		return skeletons.find(mesh->skeleton);
	}
};
} // namespace levk
