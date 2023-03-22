#pragma once
#include <levk/font/common.hpp>
#include <levk/ui/primitive.hpp>
#include <levk/util/not_null.hpp>

namespace levk {
class AsciiFont;

namespace ui {
class Text : public Primitive {
  public:
	Text(NotNull<AsciiFont*> font);

	void set_string(std::string string);
	std::string_view get_string() const { return m_string; }

	void set_height(TextHeight height);
	TextHeight get_height() const { return m_height; }

  protected:
	void refresh();

	NotNull<AsciiFont*> m_font;
	std::string m_string{};
	TextHeight m_height{TextHeight::eDefault};
};
} // namespace ui
} // namespace levk
