#pragma once
#include <experiment/component.hpp>
#include <levk/util/id.hpp>
#include <levk/util/ptr.hpp>
#include <levk/util/type_id.hpp>
#include <cassert>
#include <memory>
#include <unordered_map>
#include <vector>

namespace levk {
class Node;
}

namespace levk::experiment {
template <typename T>
concept ComponentT = std::derived_from<T, Component>;

class Entity {
  public:
	struct Renderer {
		virtual ~Renderer() = default;

		virtual void render(Entity const& entity) const = 0;
	};

	template <ComponentT T>
	T& attach(std::unique_ptr<T>&& t) {
		assert(t);
		auto& ret = *t;
		ret.m_entity = m_id;
		ret.m_scene = m_scene;
		m_components.all.insert_or_assign(TypeId::make<T>().value(), std::move(t));
		if constexpr (std::derived_from<T, TickComponent>) { m_components.tick.insert_or_assign(TypeId::make<T>().value(), &ret); }
		return ret;
	}

	template <ComponentT T>
	Ptr<T> find() const {
		if (auto it = m_components.all.find(TypeId::make<T>().value()); it != m_components.all.end()) { return &static_cast<T&>(*it->second); }
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
		m_components.all.erase(TypeId::make<T>());
		if constexpr (std::derived_from<T, TickComponent>) { m_components.tick.erase(TypeId::make<T>()); }
	}

	Id<Entity> id() const { return m_id; }
	Id<Node> node_id() const { return m_node; }
	Ptr<Renderer> renderer() const { return m_renderer.get(); }

	void tick(Time dt);

	Scene& scene() const;

  private:
	struct {
		std::unordered_map<TypeId::value_type, std::unique_ptr<Component>> all{};
		std::unordered_map<TypeId::value_type, Ptr<TickComponent>> tick{};
	} m_components{};
	struct {
		std::vector<Ptr<TickComponent>> tick{};
	} m_sorted{};
	Id<Entity> m_id{};
	Id<Node> m_node{};
	Ptr<Scene> m_scene{};
	std::unique_ptr<Renderer> m_renderer{};

	friend class Scene;
};
} // namespace levk::experiment
