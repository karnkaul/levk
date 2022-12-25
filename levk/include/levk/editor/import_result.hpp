#pragma once
#include <levk/util/id.hpp>
#include <vector>

namespace levk {
class Texture;
class Material;
struct Skeleton;
struct StaticMesh;
struct SkinnedMesh;

namespace editor {
struct ImportResult {
	std::vector<Id<Texture>> added_textures{};
	std::vector<Id<Material>> added_materials{};
	std::vector<Id<Skeleton>> added_skeletons{};
	std::vector<Id<StaticMesh>> added_static_meshes{};
	std::vector<Id<SkinnedMesh>> added_skinned_meshes{};
};
} // namespace editor
} // namespace levk
