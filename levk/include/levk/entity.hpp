#pragma once
#include <levk/transform.hpp>
#include <levk/util/id.hpp>
#include <levk/util/ptr.hpp>
#include <levk/util/type_id.hpp>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace levk {
template <typename T>
concept AttachmentT = std::is_move_constructible_v<T>;

class Entity {
  public:
	class Tree;

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
	void detach() {
		m_attachments.erase(TypeId::make<T>());
	}

	Id<Entity> id() const { return m_id; }
	Id<Entity> parent() const { return m_parent; }
	std::span<Id<Entity> const> children() const { return m_children; }

	Transform transform{};
	std::string name{};

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

	std::unordered_map<TypeId, std::unique_ptr<Base>, Hasher> m_attachments{};
	Id<Entity> m_id{};
	Id<Entity> m_parent{};
	std::vector<Id<Entity>> m_children{};
};

class Entity::Tree {
  public:
	Entity& add(Entity entity, Id<Entity> parent = {});
	void remove(Id<Entity> id);

	Ptr<Entity const> find(Id<Entity> id) const;
	Ptr<Entity> find(Id<Entity> id);

	Entity const& get(Id<Entity> id) const;
	Entity& get(Id<Entity> id);

	void reparent(Entity& out, Id<Entity> new_parent);

	std::span<Id<Entity> const> roots() const { return m_roots; }

  private:
	void set_parent_on_children(Entity& out, Id<Entity> parent);
	void remove_child_from_parent(Entity& out);

	std::unordered_map<Id<Entity>, Entity, std::hash<std::size_t>> m_entities{};
	std::vector<Id<Entity>> m_roots{};
	Id<Entity>::id_type m_next_id{};
};
} // namespace levk
