#pragma once
#include <levk/scene/component.hpp>
#include <levk/transform.hpp>
#include <levk/util/id.hpp>
#include <levk/util/ptr.hpp>
#include <levk/util/type_id.hpp>
#include <cassert>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace levk {
class Node;

template <typename T>
concept ComponentT = std::derived_from<T, Component>;

class Entity final {
  public:
	using ComponentMap = std::unordered_map<TypeId::value_type, std::unique_ptr<Component>>;

	template <ComponentT T>
	T& attach(std::unique_ptr<T>&& t) {
		assert(t);
		auto& ret = *t;
		attach(TypeId::make<T>().value(), std::move(t), std::derived_from<T, RenderComponent>);
		return ret;
	}

	template <ComponentT T>
	Ptr<T> find() const {
		if (auto it = m_components.find(TypeId::make<T>().value()); it != m_components.end()) { return &static_cast<T&>(*it->second); }
		return {};
	}

	template <ComponentT T>
	bool contains() const {
		return find<T>() != nullptr;
	}

	template <ComponentT T>
	T& get() const {
		auto ret = find<T>();
		assert(ret);
		return *ret;
	}

	template <ComponentT T>
	void detach() {
		if (auto it = m_components.find(TypeId::make<T>().value()); it != m_components.end()) {
			m_render_components.erase(it->second.get());
			m_components.erase(it);
		}
	}

	Id<Entity> id() const { return m_id; }
	Id<Node> node_id() const { return m_node; }

	bool is_destroyed() const { return m_destroyed; }
	void set_destroyed() { m_destroyed = true; }

	Transform& transform();
	Transform const& transform() const;

	void tick(Time dt);
	void render(DrawList& out) const;

	ComponentMap const& component_map() const { return m_components; }

	bool is_active{true};

  private:
	void attach(TypeId::value_type type_id, std::unique_ptr<Component>&& component, bool is_render);
	std::vector<Ptr<Component>> sorted_components() const;

	Id<Node> m_node{};
	Id<Entity> m_id{};
	Ptr<Scene> m_scene{};

	ComponentMap m_components{};
	std::unordered_map<Ptr<Component>, Ptr<RenderComponent>> m_render_components{};
	std::vector<TypeId> m_to_detach{};
	Id<Component>::id_type m_prev_id{};
	bool m_destroyed{};

	friend class Scene;
};
} // namespace levk
