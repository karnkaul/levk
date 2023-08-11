#include <fmt/format.h>
#include <levk/scene/scene.hpp>
#include <spaced/game/blueprint.hpp>

namespace spaced {
void Blueprint::create(levk::Entity& out) {
	++created;
	auto& node = out.owning_scene()->node_locator().get(out.node_id());
	node.name = indexed_name();
}

std::string Blueprint::indexed_name() const { return fmt::format("{} {}", entity_name(), created); }
} // namespace spaced
