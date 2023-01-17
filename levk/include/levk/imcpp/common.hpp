#pragma once
#include <glm/vec2.hpp>
#include <levk/util/bool.hpp>
#include <levk/util/pinned.hpp>
#include <levk/util/ptr.hpp>
#include <concepts>
#include <span>

namespace levk {
class Engine;
}

namespace levk::imcpp {
///
/// \brief Obtain the maximum width and height of a number of ImGui Texts.
/// \param strings Array of C strings to compute sizes of
/// \returns Maximum width and height (independent) of all strings
///
glm::vec2 max_size(std::span<char const* const> strings);

///
/// \brief Create a small red button.
/// \param label Label on the button
/// \returns true if clicked
///
bool small_button_red(char const* label);

///
/// \brief Create a selectable item.
/// \param label Label on the selectable
/// \param selected whether this selectable should be selected
/// \param flags ImGuiSelectableFlags (passthrough)
/// \param size Size (passthrough)
/// \returns true if clicked
///
bool selectable(char const* label, Bool selected, int flags = {}, glm::vec2 size = {});

bool input_text(char const* label, char* buffer, std::size_t size, int flags = {});

///
/// \brief RAII Dear ImGui StyleVar stack
///
class StyleVar : public Pinned {
  public:
	explicit StyleVar(int index, glm::vec2 value) { push(index, value); }
	explicit StyleVar(int index, float value) { push(index, value); }
	~StyleVar();

	void push(int index, glm::vec2 value);
	void push(int index, float value);

	explicit operator bool() const { return true; }

  private:
	int m_count{};
};

///
/// \brief Base class for RAII Dear ImGui wrappers whose widgets return a boolean on Begin()
///
class Openable : public Pinned {
  public:
	~Openable() noexcept;

	bool is_open() const { return m_open; }
	explicit operator bool() const { return is_open(); }

  protected:
	using Close = void (*)();

	Openable() = default;
	Openable(bool is_open, Close close, Bool force_close = {}) : m_close(close), m_open(is_open), m_force_close(force_close) {}

	Close m_close{};
	bool m_open{};
	bool m_force_close{};
};

///
/// \brief Required dependency on an Openable which must be open (when used as an argument)
///
template <typename T>
struct NotClosed {
	NotClosed([[maybe_unused]] T const& t) { assert(t.is_open()); }

	template <std::derived_from<T> Derived>
	NotClosed(NotClosed<Derived>) {}
};

class Canvas : public Openable {
  protected:
	using Openable::Openable;
};

///
/// \brief RAII Dear ImGui window
///
class Window : public Canvas {
  public:
	class Menu;

	explicit Window(char const* label, bool* open_if = {}, int flags = {});
	Window(NotClosed<Canvas> parent, char const* label, glm::vec2 size = {}, Bool border = {}, int flags = {});
};

using OpenWindow = NotClosed<Window>;

///
/// \brief RAII Dear ImGui TreeNode
///
class TreeNode : public Openable {
  public:
	explicit TreeNode(char const* label, int flags = {});

	static bool leaf(char const* label, int flags = {});
};

///
/// \brief Base class for Dear ImGui MenuBars
///
class MenuBar : public Openable {
  protected:
	using Openable::Openable;
};

///
/// \brief RAII Dear ImGui Menu object
///
class Menu : public Openable {
  public:
	explicit Menu(NotClosed<MenuBar>, char const* label, Bool enabled = {true});
};

///
/// \brief RAII Dear ImGui windowed MenuBar
///
class Window::Menu : public MenuBar {
  public:
	explicit Menu(NotClosed<Canvas>);
};

///
/// \brief RAII Dear ImGui MainMenuBar
///
class MainMenu : public MenuBar {
  public:
	explicit MainMenu();
};

///
/// \brief RAII Dear ImGui Popup
///
class Popup : public Canvas {
  public:
	explicit Popup(char const* id, Bool centered = {}, int flags = {}) : Popup(id, {}, {}, centered, flags) {}

	static void open(char const* id);
	static void close_current();

  protected:
	explicit Popup(char const* id, Bool modal, Bool closeable, Bool centered, int flags);
};

///
/// \brief RAII Dear ImGui PopupModal
///
class Modal : public Popup {
  public:
	explicit Modal(char const* id, Bool centered = {true}, Bool closeable = {true}, int flags = {}) : Popup(id, {true}, closeable, centered, flags) {}
};

///
/// \brief RAII Dear ImGui TabBar
///
class TabBar : public Openable {
  public:
	class Item;

	explicit TabBar(char const* label, int flags = {});
};

class TabBar::Item : public Openable {
  public:
	explicit Item(NotClosed<TabBar>, char const* label, bool* open = {}, int flags = {});
};

///
/// \brief RAII Dear ImGui Combo
///
class Combo : public Openable {
  public:
	explicit Combo(char const* label, char const* preview);

	bool item(char const* label, Bool selected, int flags = {}, glm::vec2 size = {}) const { return selectable(label, selected, flags, size); }
};

class ListBox : public Openable {
  public:
	explicit ListBox(char const* label, glm::vec2 size = {});
};
} // namespace levk::imcpp
