#include <impl/ft_lib_wrapper.hpp>

#if defined(LEVK_USE_FREETYPE)
#include <impl/ft_library.hpp>
#endif

std::unique_ptr<levk::FontLibrary> levk::make_font_library() {
#if defined(LEVK_USE_FREETYPE)
	return std::make_unique<FtLib>();
#else
	return std::make_unique<FontLibrary::Null>();
#endif
}
