#pragma once
#include <levk/material.hpp>
#include <levk/skeleton.hpp>
#include <levk/skinned_mesh.hpp>
#include <levk/static_mesh.hpp>
#include <levk/util/monotonic_map.hpp>

namespace levk {
struct MeshResources {
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
