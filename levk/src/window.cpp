#include <levk/impl/desktop_window.hpp>
#include <levk/window.hpp>

namespace levk {
Window DesktopWindowFactory::make() const { return DesktopWindow{}; }
} // namespace levk
