#pragma once
#include <levk/render_resources.hpp>
#include <levk/util/type_id.hpp>

namespace levk {
class Resources {
  public:
	Resources(std::string_view root_dir);

	Ptr<StaticMesh> load_static_mesh(Uri const& uri);
	Ptr<SkinnedMesh> load_skinned_mesh(Uri const& uri);

	bool unload_static_mesh(Uri const& uri);
	bool unload_skinned_mesh(Uri const& uri);

	std::string_view root_dir() const { return m_root_dir; }
	MeshType get_mesh_type(Uri const& uri) const;

	RenderResources render{};

  private:
	template <typename T, typename Map, typename F>
	Ptr<T> load(Uri const& uri, std::string_view const type, Map& map, F func);

	template <typename T, typename Map>
	bool unload(Uri const& uri, Map& out);

	std::string m_root_dir{};
};
} // namespace levk
