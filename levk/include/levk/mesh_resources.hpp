#pragma once
#include <levk/material.hpp>
#include <levk/mesh.hpp>
#include <levk/skeleton.hpp>
#include <levk/util/monotonic_map.hpp>

namespace levk {
struct MeshResources {
	MonotonicMap<Texture> textures{};
	MonotonicMap<Material> materials{};
	MonotonicMap<Mesh> meshes{};
	MonotonicMap<Skeleton> skeletons{};

	Ptr<Skeleton const> find_skeleton(Id<Mesh> mesh_id) const {
		auto const* mesh = meshes.find(mesh_id);
		if (!mesh) { return {}; }
		return skeletons.find(mesh->skeleton);
	}
};
} // namespace levk
