#pragma once

namespace levk::imcpp {
template <typename T>
struct NotClosed;
class Window;
using OpenWindow = NotClosed<Window>;

struct Inspectable {
	virtual void inspect(OpenWindow) = 0;
};
} // namespace levk::imcpp
