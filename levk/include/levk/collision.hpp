#pragma once
#include <levk/aabb.hpp>
#include <levk/defines.hpp>
#include <levk/util/id.hpp>
#include <levk/util/monotonic_map.hpp>
#include <levk/util/not_null.hpp>
#include <levk/util/pinned.hpp>
#include <levk/util/ptr.hpp>
#include <optional>

namespace levk {
class DrawList;

class Collision : public Pinned {
  public:
	struct Handle;
	class Scoped;

	struct Entry {
		Aabb aabb{};
		Id<Entry> colliding_with{};
	};
	using Map = MonotonicMap<Entry, Id<Aabb>>;

	Handle add(Aabb aabb);
	void remove(Id<Aabb> id);
	bool contains(Id<Aabb> id) const { return m_entries.contains(id); }

	Aabb get(Id<Aabb> id) const;
	void set(Id<Aabb> id, Aabb aabb);
	Id<Aabb> colliding_with(Id<Aabb> id) const;

	void update();

	Map const& entries() const { return m_entries; }

	bool draw_aabbs{debug_v};

  protected:
	Map m_entries{};
};

struct Collision::Handle {
	Id<Aabb> id;
	NotNull<Collision*> collision;
};

class Collision::Scoped {
  public:
	Scoped() = default;

	Scoped(Handle handle) : m_handle(handle) {}
	Scoped(Scoped&&) noexcept;
	Scoped& operator=(Scoped&& rhs) noexcept;
	~Scoped() { disconnect(); }

	void disconnect();

	Id<Aabb> id() const { return m_handle ? m_handle->id : Id<Aabb>{}; }

	explicit operator bool() const { return m_handle.has_value(); }

  private:
	std::optional<Handle> m_handle{};
};
} // namespace levk
