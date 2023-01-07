#pragma once
#include <levk/asset/uri.hpp>
#include <levk/render_resources.hpp>

namespace levk {
class Resources {
  public:
	Resources(std::string_view root_dir);

	Id<StaticMesh> load(asset::Uri<StaticMesh> const& uri);
	Id<SkinnedMesh> load(asset::Uri<SkinnedMesh> const& uri);

	std::string_view root_dir() const { return m_root_dir; }
	std::string absolute_path_for(std::string_view uri) const;
	MeshType get_mesh_type(std::string_view uri) const;

	RenderResources render{};

  private:
	template <typename T, typename Map, typename F>
	Id<T> load(asset::Uri<T> const& uri, std::string_view const type, Map const& map, F func);

	std::unordered_map<std::string, std::size_t> m_uri_to_id{};
	mutable std::mutex m_mutex{};
	std::string m_root_dir{};
};
} // namespace levk
