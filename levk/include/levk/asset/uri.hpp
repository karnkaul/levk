#pragma once
#include <levk/util/id.hpp>
#include <string>

namespace levk::asset {
template <typename Type>
using Uri = Id<Type, std::string>;
}
