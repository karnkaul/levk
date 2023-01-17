#pragma once
#include <levk/render_resources.hpp>
#include <levk/util/reader.hpp>
#include <levk/util/type_id.hpp>
#include <atomic>

namespace levk {
class Resources {
  public:
	Resources(Reader& reader);

	Ptr<StaticMesh> load_static_mesh(Uri const& uri);
	Ptr<SkinnedMesh> load_skinned_mesh(Uri const& uri);

	bool unload_static_mesh(Uri const& uri);
	bool unload_skinned_mesh(Uri const& uri);

	MeshType get_mesh_type(Uri const& uri) const;

	std::uint64_t signature() const { return m_signature.load(); }
	Reader& reader() const { return *m_reader; }

	RenderResources render{};

  private:
	template <typename T, typename Map, typename F>
	Ptr<T> load(Uri const& uri, std::string_view const type, Map& map, F func);

	template <typename T, typename Map>
	bool unload(Uri const& uri, Map& out);

	Ptr<Reader> m_reader{};
	std::atomic_uint64_t m_signature{};
};
} // namespace levk
