#pragma once
#include <levk/serializable.hpp>
#include <levk/util/type_id.hpp>
#include <levk/util/unique_task.hpp>
#include <memory>
#include <unordered_map>

namespace levk::experiment {
class Serializer {
  public:
	using Factory = UniqueTask<std::unique_ptr<ISerializable>()>;

	template <typename Type>
		requires(std::is_default_constructible_v<Type>)
	void bind_to(std::string type_name) {
		bind_to(std::move(type_name), [] { return std::make_unique<Type>(); });
	}

	void bind_to(std::string type_name, Factory factory);

	dj::Json serialize(ISerializable const& serializable) const;
	std::unique_ptr<ISerializable> deserialize(dj::Json const& json) const;

  private:
	std::unordered_map<std::string, Factory> m_factories{};
};
} // namespace levk::experiment
