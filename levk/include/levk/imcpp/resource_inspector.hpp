#pragma once
#include <levk/imcpp/common.hpp>
#include <levk/resources.hpp>

namespace levk::imcpp {
struct ResourceInspector {
	struct Inspect {
		TypeId type{};
		Uri uri{};

		explicit operator bool() const { return type != TypeId{} && uri; }
	};

	Inspect inspecting{};
	TypeId resource_type{};

	void inspect(NotClosed<Window> window, Resources& out);
};
} // namespace levk::imcpp
