#pragma once
#include <levk/imcpp/common.hpp>

namespace levk {
struct Inspectable {
	virtual ~Inspectable() = default;

	virtual void inspect(imcpp::OpenWindow w);
};
} // namespace levk
