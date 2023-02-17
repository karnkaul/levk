#pragma once
#include <levk/imcpp/common.hpp>
#include <levk/resources.hpp>
#include <levk/util/path_tree.hpp>

namespace levk::imcpp {
class ResourceDisplay {
  public:
	struct Inspect {
		Uri<> uri{};
		TypeId type{};

		explicit operator bool() const { return type != TypeId{} && uri; }
	};

	void draw_to(NotClosed<Window> window, Resources& out);

	Inspect inspecting{};
	TypeId resource_type{};

  private:
	struct Interact {
		std::string interacted{};
		bool right_clicked{};
		bool double_clicked{};
	};

	template <typename T>
	void list_resources(std::string_view type_name, ResourceMap<T>& map, PathTree& out_tree, std::uint64_t signature);
	void list_resources(Resources& resources, TypeId type);

	bool walk(std::string_view type_name, PathTree const& path_tree);

	struct {
		PathTree textures{};
		PathTree materials{};
		PathTree static_meshes{};
		PathTree skinned_meshes{};
		PathTree skeletons{};
	} m_trees{};
	Interact m_interact{};
	std::uint64_t m_signature{};
};
} // namespace levk::imcpp
