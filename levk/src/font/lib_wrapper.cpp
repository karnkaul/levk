#include <font/lib_wrapper.hpp>

#if defined(LEVK_USE_FREETYPE)
#include <font/freetype/library.hpp>
#endif

std::unique_ptr<levk::FontLibrary> levk::make_font_library() {
#if defined(LEVK_USE_FREETYPE)
	return std::make_unique<Freetype>();
#else
	return std::make_unique<FontLibrary::Null>();
#endif
}
