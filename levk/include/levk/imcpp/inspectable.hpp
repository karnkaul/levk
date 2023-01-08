#pragma once

namespace levk::imcpp {
template <typename T>
class NotClosed;
class Window;
using OpenWindow = NotClosed<Window>;

struct Inspectable {
	virtual void inspect(OpenWindow) = 0;
};
} // namespace levk::imcpp
