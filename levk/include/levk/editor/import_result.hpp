#pragma once
#include <levk/util/id.hpp>
#include <vector>

namespace levk {
class Texture;
class Material;
struct Skeleton;
class Mesh;

namespace editor {
struct ImportResult {
	std::vector<Id<Texture>> added_textures{};
	std::vector<Id<Material>> added_materials{};
	std::vector<Id<Skeleton>> added_skeletons{};
	std::vector<Id<Mesh>> added_meshes{};
};
} // namespace editor
} // namespace levk
