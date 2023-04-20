#pragma once
#include <levk/util/id.hpp>
#include <levk/util/ptr.hpp>
#include <cassert>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace levk {
template <typename Type, typename IdType = Id<Type>>
class MonotonicMap {
  public:
	using id_type = IdType;
	using id_underlying_t = typename IdType::id_type;

	std::pair<IdType, Type&> add(Type t = Type{}) {
		auto const id = ++m_prev_id;
		auto& ret = *m_map.insert_or_assign(id, std::move(t)).first;
		return {ret.first, ret.second};
	}

	void remove(IdType id) { m_map.erase(id); }

	bool contains(IdType id) const { return m_map.contains(id); }

	Ptr<Type const> find(IdType id) const {
		if (auto it = m_map.find(id); it != m_map.end()) { return &it->second; }
		return {};
	}

	Ptr<Type> find(IdType id) { return const_cast<Type*>(std::as_const(*this).find(id)); }

	Type const& get(IdType id) const {
		auto ret = find(id);
		assert(ret);
		return *ret;
	}

	Type& get(IdType id) { return const_cast<Type&>(std::as_const(*this).get(id)); }

	Type const& get_or(IdType id, Type const& fallback) const {
		if (auto* ret = find(id)) { return *ret; }
		return fallback;
	}

	std::size_t size() const { return m_map.size(); }

	bool empty() const { return m_map.empty(); }

	void clear() { m_map.clear(); }

	template <typename Func>
	void remove_if(Func&& predicate) {
		auto to_remove = std::vector<id_underlying_t>{};
		for (auto& [key, value] : m_map) {
			if (predicate(key, value)) { to_remove.push_back(key); }
		}
		for (auto const key : to_remove) { m_map.erase(key); }
	}

	auto begin() { return m_map.begin(); }
	auto end() { return m_map.end(); }
	auto begin() const { return m_map.begin(); }
	auto end() const { return m_map.end(); }

  private:
	std::unordered_map<id_underlying_t, Type> m_map{};
	mutable std::mutex m_mutex{};
	id_underlying_t m_prev_id{};
};
} // namespace levk
