#pragma once
#include <levk/util/id.hpp>
#include <levk/util/ptr.hpp>
#include <levk/util/type_id.hpp>
#include <memory>
#include <unordered_map>

namespace levk {
template <typename T>
concept AttachmentT = std::is_move_constructible_v<T>;

template <typename Type, typename IdType>
class MonotonicMap;

class Entity {
  public:
	using Map = MonotonicMap<Entity, std::size_t>;

	template <AttachmentT T>
	T& attach(T t) {
		auto ut = std::make_unique<Model<T>>(std::move(t));
		auto& ret = ut->t;
		m_attachments.insert_or_assign(TypeId::make<T>(), std::move(ut));
		return ret;
	}

	template <AttachmentT T>
	Ptr<T> find() const {
		if (auto it = m_attachments.find(TypeId::make<T>()); it != m_attachments.end()) { return &static_cast<Model<T>&>(*it->second).t; }
		return {};
	}

	template <AttachmentT T>
	T& get() const {
		auto ret = find<T>();
		assert(ret);
		return *ret;
	}

	template <AttachmentT T>
	void detach() {
		m_attachments.erase(TypeId::make<T>());
	}

	Id<Entity> id() const { return m_id; }

  private:
	struct Base {
		virtual ~Base() = default;
	};
	template <typename T>
	struct Model : Base {
		T t;
		Model(T&& t) : t(std::move(t)) {}
	};
	struct Hasher {
		std::size_t operator()(TypeId const& type) const { return std::hash<std::size_t>{}(type.value()); }
	};

	void set_id(Id<Entity> id) { m_id = id; }

	std::unordered_map<TypeId, std::unique_ptr<Base>, Hasher> m_attachments{};
	Id<Entity> m_id{};

	friend class MonotonicMap<Entity, std::size_t>;
};
} // namespace levk
