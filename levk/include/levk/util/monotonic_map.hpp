#pragma once
#include <levk/util/id.hpp>
#include <levk/util/ptr.hpp>
#include <cassert>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace levk {
template <typename Type>
concept IdSettableT = requires(Type& t, Id<Type> id) { t.set_id(id); };

template <typename Type, typename IdType = Id<Type>>
class MonotonicMap {
  public:
	using id_type = IdType;

	std::pair<Id<Type>, Type&> add(Type t = Type{}) {
		auto lock = std::scoped_lock{m_mutex};
		auto const id = ++m_prev_id;
		if constexpr (IdSettableT<Type>) { t.set_id(id); }
		auto& ret = *m_map.insert_or_assign(id, std::move(t)).first;
		return {ret.first, ret.second};
	}

	void remove(Id<Type> id) {
		auto lock = std::scoped_lock{m_mutex};
		m_map.erase(id);
	}

	bool contains(Id<Type> id) const {
		auto lock = std::scoped_lock{m_mutex};
		return m_map.contains(id);
	}

	Ptr<Type const> find(Id<Type> id) const {
		auto lock = std::scoped_lock{m_mutex};
		if (auto it = m_map.find(id); it != m_map.end()) { return &it->second; }
		return {};
	}

	Ptr<Type> find(Id<Type> id) { return const_cast<Type*>(std::as_const(*this).find(id)); }

	Type const& get(Id<Type> id) const {
		auto ret = find(id);
		assert(ret);
		return *ret;
	}

	Type& get(Id<Type> id) { return const_cast<Type&>(std::as_const(*this).get(id)); }

	Type const& get_or(Id<Type> id, Type const& fallback) const {
		if (auto* ret = find(id)) { return *ret; }
		return fallback;
	}

	std::size_t size() const {
		auto lock = std::scoped_lock{m_mutex};
		return m_map.size();
	}

	bool empty() const {
		auto lock = std::scoped_lock{m_mutex};
		return m_map.empty();
	}

	void clear() {
		auto lock = std::scoped_lock{m_mutex};
		m_map.clear();
	}

	template <typename F>
	void for_each(F&& func) {
		auto lock = std::scoped_lock{m_mutex};
		for (auto& [key, value] : m_map) { func(key, value); }
	}

	template <typename F>
	void for_each(F&& func) const {
		auto lock = std::scoped_lock{m_mutex};
		for (auto& [key, value] : m_map) { func(key, value); }
	}

  private:
	using id_underlying_t = typename IdType::id_type;
	std::unordered_map<id_underlying_t, Type> m_map{};
	mutable std::mutex m_mutex{};
	id_underlying_t m_prev_id{};
};
} // namespace levk
