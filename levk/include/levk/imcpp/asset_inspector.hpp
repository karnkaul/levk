#pragma once
#include <levk/asset/asset_providers.hpp>
#include <levk/imcpp/common.hpp>
#include <levk/util/path_tree.hpp>

namespace levk::imcpp {
class AssetInspector {
  public:
	struct Inspect {
		Uri<> uri{};
		TypeId type{};

		explicit operator bool() const { return type != TypeId{} && uri; }
	};

	void draw_to(NotClosed<Window> window, AssetProviders& out);

	Inspect inspecting{};
	TypeId resource_type{};

  private:
	struct Interact {
		std::string interacted{};
		bool right_clicked{};
		bool double_clicked{};
	};

	template <typename T>
	void list_assets(std::string_view type_name, AssetProvider<T>& provider, PathTree& out_tree);
	void list_assets(AssetProviders& providers, TypeId type);

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
