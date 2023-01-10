#pragma once
#include <levk/material.hpp>
#include <levk/skeleton.hpp>
#include <levk/skinned_mesh.hpp>
#include <levk/static_mesh.hpp>
#include <levk/util/monotonic_map.hpp>

namespace levk {
struct RenderResources {
	MonotonicMap<Texture> textures{};
	MonotonicMap<Material> materials{};
	MonotonicMap<StaticMesh> static_meshes{};
	MonotonicMap<SkinnedMesh> skinned_meshes{};
	MonotonicMap<Skeleton> skeletons{};

	Ptr<Skeleton const> find_skeleton(Id<SkinnedMesh> mesh_id) const {
		auto const* mesh = skinned_meshes.find(mesh_id);
		if (!mesh) { return {}; }
		return skeletons.find(mesh->skeleton);
	}
};
} // namespace levk

#include <levk/resource_map.hpp>

namespace levk::refactor {
struct RenderResources {
	ResourceMap<Texture> textures{};
	ResourceMap<Material> materials{};
	ResourceMap<StaticMesh> static_meshes{};
	ResourceMap<SkinnedMesh> skinned_meshes{};
	ResourceMap<Skeleton> skeletons{};

	Ptr<Skeleton const> find_skeleton(Uri const& mesh_uri) const {
		auto const* mesh = skinned_meshes.find(mesh_uri);
		if (!mesh) { return {}; }
		return skeletons.find(mesh->skeleton);
	}
};
} // namespace levk::refactor
