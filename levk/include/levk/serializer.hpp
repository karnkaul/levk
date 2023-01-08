#pragma once
#include <levk/component.hpp>
#include <levk/util/bool.hpp>
#include <levk/util/type_id.hpp>
#include <levk/util/unique_task.hpp>
#include <memory>
#include <unordered_map>

namespace levk {
class Serializer {
  public:
	template <std::derived_from<Serializable> Type>
	using Factory = UniqueTask<std::unique_ptr<Type>()>;

	struct Entry {
		Factory<Serializable> factory{};
		TypeId type_id{};
		bool is_component{};
	};

	using Map = std::unordered_map<std::string, Entry>;

	template <std::derived_from<Serializable> Type>
	struct Result {
		std::unique_ptr<Type> value{};
		TypeId type_id{};
		bool is_component{};

		explicit operator bool() const { return type_id != TypeId{} && value != nullptr; }
	};

	template <std::derived_from<Serializable> To>
	static std::unique_ptr<To> dynamic_pointer_cast(std::unique_ptr<Serializable>&& in) {
		return std::unique_ptr<To>{dynamic_cast<To*>(in.release())};
	}

	template <std::derived_from<Serializable> Type>
		requires(std::is_default_constructible_v<Type>)
	void bind() {
		bind_to<Type>(Type{}.type_name(), [] { return std::make_unique<Type>(); });
	}

	template <std::derived_from<Serializable> Type>
	void bind_to(Factory<Type> factory) {
		auto const type_name = factory()->type_name();
		bind_to(type_name, std::move(factory));
	}

	dj::Json serialize(Serializable const& serializable) const;
	Result<Serializable> deserialize(dj::Json const& json) const;

	template <std::derived_from<Serializable> To>
	Result<To> deserialize_as(dj::Json const& json) const {
		auto result = deserialize(json);
		return {dynamic_pointer_cast<To>(std::move(result.value)), result.type_id, result.is_component};
	}

	TypeId type_id(std::string const& type_name) const;

	Ptr<Entry const> find_entry(std::string const& type_name) const;
	Map const& entries() const { return m_entries; }

  private:
	template <typename Type>
	void bind_to(std::string_view type_name, Factory<Type> factory) {
		bind_to(std::string{type_name}, TypeId::make<Type>(), std::move(factory), {std::derived_from<Type, Component>});
	}

	void bind_to(std::string&& type_name, TypeId type_id, Factory<Serializable>&& factory, Bool is_component);

	Map m_entries{};
};
} // namespace levk
