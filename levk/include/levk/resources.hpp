#pragma once
#include <levk/asset/uri.hpp>
#include <levk/render_resources.hpp>
#include <levk/util/type_id.hpp>

namespace levk {
class Resources {
  public:
	struct Entry {
		Id<void>::id_type id{};
		TypeId type_id{};
	};

	Resources(std::string_view root_dir);

	Id<StaticMesh> load(asset::Uri<StaticMesh> const& uri);
	Id<SkinnedMesh> load(asset::Uri<SkinnedMesh> const& uri);

	bool unload(asset::Uri<StaticMesh> const& uri);
	bool unload(asset::Uri<SkinnedMesh> const& uri);

	std::string_view root_dir() const { return m_root_dir; }
	std::string absolute_path_for(std::string_view uri) const;
	MeshType get_mesh_type(std::string_view uri) const;

	template <typename Func>
	void for_each_entry(Func&& func) const {
		auto lock = std::scoped_lock{m_mutex};
		for (auto const& [uri, entry] : m_uri_to_entry) { func(uri, entry); }
	}

	std::uint64_t signature() const;

	RenderResources render{};

  private:
	template <typename T, typename Map, typename F>
	Id<T> load(asset::Uri<T> const& uri, std::string_view const type, Map const& map, F func);

	template <typename T, typename Map>
	bool unload(asset::Uri<T> const& uri, Map& out);

	std::unordered_map<std::string, Entry> m_uri_to_entry{};
	mutable std::mutex m_mutex{};
	std::string m_root_dir{};
	std::uint64_t m_signature{};
};
} // namespace levk

namespace levk::refactor {
class Resources {
  public:
	Resources(std::string_view root_dir);

	Ptr<StaticMesh> load_static_mesh(Uri const& uri);
	// Ptr<SkinnedMesh> load_skinned_mesh(Uri const& uri);

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
} // namespace levk::refactor
