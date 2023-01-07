#pragma once
#include <levk/component.hpp>
#include <levk/serializable.hpp>
#include <levk/util/type_id.hpp>
#include <levk/util/unique_task.hpp>
#include <memory>
#include <unordered_map>

namespace levk {
class Serializer {
  public:
	template <std::derived_from<ISerializable> Type>
	using Factory = UniqueTask<std::unique_ptr<Type>()>;

	struct Entry {
		Factory<ISerializable> factory{};
		TypeId type_id{};
		bool is_component{};
	};

	using Map = std::unordered_map<std::string, Entry>;

	template <std::derived_from<ISerializable> Type>
	struct Result {
		std::unique_ptr<Type> value{};
		TypeId type_id{};
		bool is_component{};

		explicit operator bool() const { return type_id != TypeId{} && value != nullptr; }
	};

	template <std::derived_from<ISerializable> To>
	static std::unique_ptr<To> dynamic_pointer_cast(std::unique_ptr<ISerializable>&& in) {
		return std::unique_ptr<To>{dynamic_cast<To*>(in.release())};
	}

	template <typename Type>
		requires(std::is_default_constructible_v<Type>)
	void bind_to(std::string type_name) {
		bind_to<Type>(std::move(type_name), [] { return std::make_unique<Type>(); });
	}

	template <typename Type>
	void bind_to(std::string type_name, Factory<Type> factory) {
		bind_to(std::move(type_name), TypeId::make<Type>(), std::move(factory), std::derived_from<Type, Component>);
	}

	dj::Json serialize(ISerializable const& serializable) const;
	Result<ISerializable> deserialize(dj::Json const& json) const;

	template <std::derived_from<ISerializable> To>
	Result<To> deserialize_as(dj::Json const& json) const {
		auto result = deserialize(json);
		return {dynamic_pointer_cast<To>(std::move(result.value)), result.type_id, result.is_component};
	}

	TypeId type_id(std::string const& type_name) const;

	Ptr<Entry const> find_entry(std::string const& type_name) const;
	Map const& entries() const { return m_entries; }

  private:
	void bind_to(std::string&& type_name, TypeId type_id, Factory<ISerializable>&& factory, bool is_component);

	Map m_entries{};
};
} // namespace levk
