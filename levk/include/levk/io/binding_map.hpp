#pragma once
#include <levk/util/logger.hpp>
#include <levk/util/type_id.hpp>
#include <cassert>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace levk {
template <typename Base>
class BindingMap {
  public:
	template <typename Type>
	using Factory = std::function<std::unique_ptr<Type>()>;

	template <typename Type>
	struct Result {
		std::unique_ptr<Type> value{};
		TypeId type_id{};

		explicit operator bool() const { return type_id != TypeId{} && value != nullptr; }
	};

	template <std::derived_from<Base> To>
	static std::unique_ptr<To> dynamic_pointer_cast(std::unique_ptr<Base>&& in) {
		return std::unique_ptr<To>{dynamic_cast<To*>(in.release())};
	}

	// clang-format off
	template <std::derived_from<Base> Type>
		requires(std::is_default_constructible_v<Type>) 
	bool bind() {
		// clang-format on
		return bind_to<Type>(Type{}.type_name(), [] { return std::make_unique<Type>(); });
	}

	template <std::derived_from<Base> Type>
	bool bind_to(Factory<Type> factory) {
		if (!factory) { return false; }
		auto const temp = factory();
		if (!temp) { return false; }
		return bind_to(temp->type_name(), std::move(factory));
	}

	TypeId type_id(std::string const& type_name) const {
		if (auto it = m_entries.find(type_name); it != m_entries.end()) { return it->second.type_id; }
		return {};
	}

	std::unordered_set<std::string> const& type_names() const { return m_type_names; }

	template <typename Type>
	std::unique_ptr<Type> try_make(std::string const& type_name) const {
		if (auto it = m_entries.find(type_name); it != m_entries.end()) { return dynamic_pointer_cast<Type>(it->second.factory()); }
		return {};
	}

	template <typename Type>
	bool bind_to(std::string_view type_name, Factory<Type> factory) {
		return bind_to(std::string{type_name}, TypeId::make<Type>(), std::move(factory));
	}

	virtual bool bind_to(std::string type_name, TypeId type_id, Factory<Base> factory) {
		if (type_name.empty()) {
			m_logger.warn("Ignoring attempt to bind empty type_name");
			return false;
		}
		if (!factory) {
			m_logger.warn("Ignoring attempt to bind null Factory to type_name [{}]", type_name);
			return false;
		}
		if (type_id == TypeId{}) {
			m_logger.warn("Ignoring attempt to bind null TypeId to type_name [{}]", type_name);
			return false;
		}
		m_entries.insert_or_assign(type_name, Entry{std::move(factory), type_id});
		m_type_names.insert(type_name);
		return true;
	}

  protected:
	struct Entry {
		Factory<Base> factory{};
		TypeId type_id{};
	};

	std::unordered_map<std::string, Entry> m_entries{};
	std::unordered_set<std::string> m_type_names{};
	Logger m_logger{"BindingMap"};
};
} // namespace levk
