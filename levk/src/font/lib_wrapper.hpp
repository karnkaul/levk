#pragma once
#include <levk/font/font_library.hpp>

namespace levk {
std::unique_ptr<FontLibrary> make_font_library();
} // namespace levk
