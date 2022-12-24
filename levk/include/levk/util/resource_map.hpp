#pragma once
#include <levk/util/error.hpp>
#include <levk/util/id.hpp>
#include <levk/util/ptr.hpp>
#include <cassert>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace levk {
template <typename Type>
using Uri = Id<Type, std::string>;

template <typename Type>
class ResourceMap {
  public:
	Type& add(std::string uri, Type&& resource) {
		assert(!uri.empty());
		auto lock = std::scoped_lock{m_mutex};
		auto [it, _] = m_map.insert_or_assign(std::move(uri), std::move(resource));
		return it->second;
	}

	Ptr<Type const> find(std::string const& uri) const {
		if (uri.empty()) { return {}; }
		auto lock = std::scoped_lock{m_mutex};
		if (auto it = m_map.find(uri); it != m_map.end()) { return &it->second; }
		return {};
	}

	Ptr<Type> find(std::string const& uri) { return const_cast<Type*>(std::as_const(*this).find(uri)); }

	Type const& get(std::string const& uri) const noexcept(false) {
		assert(!uri.empty());
		if (auto* ret = find(uri)) { return *ret; }
		throw Error{"Resource [" + uri + "] not found"};
	}

	Type& get(std::string const& uri) { return const_cast<Type&>(std::as_const(*this).get(uri)); }

	Type const& get_or(std::string const& uri, Type const& fallback) const {
		if (auto const* ret = find(uri)) { return *ret; }
		return fallback;
	}

	bool contains(std::string const& uri) const { return find(uri) != nullptr; }

	void remove(std::string const& uri) {
		if (uri.empty()) { return; }
		auto lock = std::scoped_lock{m_mutex};
		m_map.erase(uri);
	}

	Type const& operator[](std::string const& uri) const noexcept(false) { return get(uri); }
	Type& operator[](std::string const& uri) noexcept(false) { return get(uri); }

  private:
	std::unordered_map<std::string, Type> m_map{};
	mutable std::mutex m_mutex{};
};
} // namespace levk
