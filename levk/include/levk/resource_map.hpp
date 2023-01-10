#pragma once
#include <levk/uri.hpp>
#include <levk/util/ptr.hpp>
#include <cassert>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace levk {
template <typename Type>
class ResourceMap {
  public:
	Type& add(Uri uri, Type t = Type{}) {
		auto lock = std::scoped_lock{m_mutex};
		auto& ret = *m_map.insert_or_assign(std::move(uri), std::move(t)).first;
		return ret.second;
	}

	void remove(Uri const& uri) {
		auto lock = std::scoped_lock{m_mutex};
		m_map.erase(uri);
	}

	bool contains(Uri const& uri) const {
		auto lock = std::scoped_lock{m_mutex};
		return m_map.contains(uri);
	}

	Ptr<Type const> find(Uri const& uri) const {
		auto lock = std::scoped_lock{m_mutex};
		if (auto it = m_map.find(uri); it != m_map.end()) { return &it->second; }
		return {};
	}

	Ptr<Type> find(Uri const& uri) { return const_cast<Type*>(std::as_const(*this).find(uri)); }

	Type const& get(Uri const& uri) const {
		auto ret = find(uri);
		assert(ret);
		return *ret;
	}

	Type& get(Uri const& uri) { return const_cast<Type&>(std::as_const(*this).get(uri)); }

	Type const& get_or(Uri const& uri, Type const& fallback) const {
		if (auto* ret = find(uri)) { return *ret; }
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

	void import_map(std::unordered_map<Uri, Type, Uri::Hasher> map) {
		auto lock = std::scoped_lock{m_mutex};
		m_map = std::move(map);
	}

  private:
	std::unordered_map<Uri, Type, Uri::Hasher> m_map{};
	mutable std::mutex m_mutex{};
};
} // namespace levk
