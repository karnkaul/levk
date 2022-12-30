#pragma once
#include <levk/skinned_mesh.hpp>
#include <levk/static_mesh.hpp>
#include <variant>

namespace levk {
using Mesh = std::variant<StaticMesh, SkinnedMesh>;
}
