#pragma once
#include <levk/scene/component.hpp>
#include <levk/util/bit_flags.hpp>
#include <levk/util/type_id.hpp>

namespace levk {
class Serializer {
  public:
	enum class Tag : std::uint8_t {
		eComponent = 1 << 0,
	};
	using Tags = BitFlags<Tag>;

	template <typename Type>
	using Factory = std::function<std::unique_ptr<Type>()>;

	template <typename Type>
	struct Result {
		std::unique_ptr<Type> value{};
		std::string_view type_name{};
		TypeId type_id{};

		explicit operator bool() const { return type_id != TypeId{} && value != nullptr; }
	};

	template <std::derived_from<Serializable> To>
	static std::unique_ptr<To> dynamic_unique_cast(std::unique_ptr<Serializable>&& in) {
		if (auto* p = dynamic_cast<To*>(in.get())) {
			auto ret = std::unique_ptr<To>{p};
			static_cast<void>(in.release());
			return ret;
		}
		return {};
	}

	template <std::derived_from<Serializable> To>
	static constexpr Tags build_tags() {
		auto ret = Tags{};
		if constexpr (std::derived_from<To, Component>) { ret.set(Tag::eComponent); }
		return ret;
	}

	// clang-format off
	template <std::derived_from<Serializable> Type>
		requires(std::is_default_constructible_v<Type>) 
	bool bind() {
		// clang-format on
		return bind_to<Type>(std::string{Type{}.type_name()}, [] { return std::make_unique<Type>(); });
	}

	template <std::derived_from<Serializable> Type>
	bool bind_to(Factory<Type> factory) {
		if (!factory) { return false; }
		auto const temp = factory();
		if (!temp) { return false; }
		return bind_to(temp->type_name(), std::move(factory));
	}

	template <std::derived_from<Serializable> Type>
	bool bind_to(std::string type_name, Factory<Type> factory) {
		return bind_to(std::move(type_name), TypeId::make<Type>(), std::move(factory), build_tags<Type>());
	}

	bool bind_to(std::string type_name, TypeId type_id, Factory<Serializable> factory, Tags tags);

	template <typename Type>
	std::unique_ptr<Type> try_make(std::string const& type_name) const {
		if (auto it = m_entries.find(type_name); it != m_entries.end()) { return dynamic_unique_cast<Type>(it->second.factory()); }
		return {};
	}

	TypeId type_id(std::string const& type_name) const;
	std::unordered_set<std::string> const& type_names() const { return m_type_names; }
	std::vector<std::string_view> type_names_by_tag(Tag tag) const;

	dj::Json serialize(Serializable const& serializable) const;
	Result<Serializable> deserialize(dj::Json const& json) const;

	template <std::derived_from<Serializable> To>
	Result<To> deserialize_as(dj::Json const& json) const {
		auto result = deserialize(json);
		return {dynamic_unique_cast<To>(std::move(result.value)), result.type_name, result.type_id};
	}

	bool attach(Entity& out, std::string const& type_name) const;

  protected:
	struct Entry {
		Factory<Serializable> factory{};
		TypeId type_id{};
		Tags tags{};
	};

	std::unordered_map<std::string, Entry> m_entries{};
	std::unordered_set<std::string> m_type_names{};
};
} // namespace levk
