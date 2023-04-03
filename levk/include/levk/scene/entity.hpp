#pragma once
#include <levk/scene/component.hpp>
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
		attach(TypeId::make<T>().value(), std::move(t));
		return ret;
	}

	template <ComponentT T>
	Ptr<T> find() const {
		if (auto it = m_components.find(TypeId::make<T>().value()); it != m_components.end()) { return &static_cast<T&>(*it->second); }
		return {};
	}

	template <ComponentT T>
	T& get() const {
		auto ret = find<T>();
		assert(ret);
		return *ret;
	}

	template <ComponentT T>
	void detach() {
		if (auto it = m_components.find(TypeId::make<T>()); it != m_components.end()) {
			m_render_components.erase(it->second.get());
			m_components.erase(it);
		}
	}

	Id<Entity> id() const { return m_id; }
	Id<Node> node_id() const { return m_node; }

	void tick(Time dt);

	ComponentMap const& component_map() const { return m_components; }

	bool active{true};

  private:
	void init(Component& out) const;
	void attach(TypeId::value_type type_id, std::unique_ptr<Component>&& component);

	ComponentMap m_components{};
	std::unordered_set<Ptr<RenderComponent>> m_render_components{};
	std::vector<Ptr<Component>> m_sorted{};
	Id<Entity> m_id{};
	Id<Node> m_node{};
	Ptr<Scene> m_scene{};

	friend class Scene;
};
} // namespace levk
