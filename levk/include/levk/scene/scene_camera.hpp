#pragma once
#include <levk/graphics/camera.hpp>
#include <levk/util/id.hpp>

namespace levk {
class Entity;

struct SceneCamera : Camera {
	///
	/// \brief Target entity whose transform to use.
	///
	/// Scene copies the target transform (if it exists) into the camera at the end of every tick.
	///
	Id<Entity> target{};
};
} // namespace levk
