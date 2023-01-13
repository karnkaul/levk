#pragma once
#include <levk/imcpp/editor_window.hpp>
#include <variant>
#include <vector>

namespace levk {
struct MainMenu {
	struct Separator {};

	struct List {
		std::string label{};
		std::function<void()> callback{};
	};

	struct Custom {
		std::string label{};
		std::function<void(bool& show)> draw{};

		Bool show{};
	};

	using WindowItem = std::variant<imcpp::EditorWindow, Separator, Custom>;

	enum class Action { eNone, eExit };

	struct Result {
		Action action{};
	};

	struct {
		std::vector<List> lists{};
		std::vector<WindowItem> window{};
	} menus{};

	Result display_menu();
	void display_windows();
};
} // namespace levk
