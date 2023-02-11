#pragma once
#include <levk/render_resources.hpp>
#include <levk/util/reader.hpp>
#include <levk/util/type_id.hpp>
#include <atomic>

namespace levk {
class Resources {
  public:
	Resources(Reader& reader);

	Ptr<StaticMesh const> load(Uri<StaticMesh> const& uri);
	Ptr<SkinnedMesh const> load(Uri<SkinnedMesh> const& uri);

	bool unload(Uri<StaticMesh> const& uri);
	bool unload(Uri<SkinnedMesh> const& uri);

	MeshType get_mesh_type(Uri<> const& uri) const;

	std::uint64_t signature() const { return m_signature.load(); }
	Reader& reader() const { return *m_reader; }

	RenderResources render{};

  private:
	template <typename T, typename F>
	Ptr<T const> load(Uri<> const& uri, std::string_view const type, F func);

	template <typename T, typename Map>
	bool unload(Uri<> const& uri, Map& out);

	Ptr<Reader> m_reader{};
	std::atomic_uint64_t m_signature{};
};
} // namespace levk
