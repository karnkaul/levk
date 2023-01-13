#pragma once
#include <levk/imcpp/editor_window.hpp>
#include <variant>
#include <vector>

namespace levk {
struct MainMenu {
	struct Separator {};

	struct Custom {
		std::string label{};
		std::function<void(bool& show)> draw{};

		Bool show{};
	};

	using Entry = std::variant<imcpp::EditorWindow, Separator, Custom>;

	enum class Action { eNone, eExit };

	struct Result {
		Action action{};
	};

	std::vector<Entry> entries{};

	Result display_menu();
	void display_windows();
};
} // namespace levk
