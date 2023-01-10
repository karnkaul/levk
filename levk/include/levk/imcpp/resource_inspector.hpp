#pragma once
#include <levk/imcpp/common.hpp>
#include <levk/resources.hpp>
#include <levk/util/path_tree.hpp>

namespace levk::imcpp {
class ResourceInspector {
  public:
	struct Inspect {
		Uri uri{};
		TypeId type{};

		explicit operator bool() const { return type != TypeId{} && uri; }
	};

	void inspect(NotClosed<Window> window, Resources& out);

	Inspect inspecting{};
	TypeId resource_type{};

  private:
	template <typename T>
	void list_resources(std::string_view type_name, ResourceMap<T>& map, PathTree& out_tree, std::uint64_t signature);
	void list_resources(Resources& resources, TypeId type);

	struct {
		PathTree textures{};
		PathTree materials{};
		PathTree static_meshes{};
		PathTree skinned_meshes{};
		PathTree skeletons{};
	} m_trees{};
	std::string m_right_clicked{};
	std::uint64_t m_signature{};
};
} // namespace levk::imcpp
