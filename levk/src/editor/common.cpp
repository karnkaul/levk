#include <imgui.h>
#include <levk/editor/common.hpp>
#include <cassert>

namespace levk::editor {
glm::vec2 max_size(std::span<char const* const> strings) {
	auto ret = glm::vec2{};
	for (auto const* str : strings) {
		auto const size = ImGui::CalcTextSize(str);
		ret.x = std::max(ret.x, size.x);
		ret.y = std::max(ret.y, size.y);
	}
	return ret;
}

bool small_button_red(char const* label) {
	bool ret = false;
	ImGui::PushStyleColor(ImGuiCol_Button, {0.8f, 0.0f, 0.1f, 1.0f});
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {1.0f, 0.0f, 0.1f, 1.0f});
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.6f, 0.0f, 0.1f, 1.0f});
	if (ImGui::SmallButton(label)) { ret = true; }
	ImGui::PopStyleColor(3);
	return ret;
}

bool selectable(char const* label, Bool selected, int flags, glm::vec2 size) { return ImGui::Selectable(label, selected.value, flags, {size.x, size.y}); }

Openable::~Openable() noexcept {
	if (m_open || m_force_close) {
		assert(m_close);
		(*m_close)();
	}
}

Window::Window(char const* label, bool* open_if, int flags) : Canvas(ImGui::Begin(label, open_if, flags), &ImGui::End, {true}) {}

Window::Window(NotClosed<Canvas>, char const* label, glm::vec2 size, Bool border, int flags)
	: Canvas(ImGui::BeginChild(label, {size.x, size.y}, border.value, flags), &ImGui::EndChild, {true}) {}

TreeNode::TreeNode(char const* label, int flags) : Openable(ImGui::TreeNodeEx(label, flags), &ImGui::TreePop) {}

bool TreeNode::leaf(char const* label, int flags) {
	auto tn = TreeNode{label, flags | ImGuiTreeNodeFlags_Leaf};
	return ImGui::IsItemClicked();
}

Window::Menu::Menu(NotClosed<Canvas>) : MenuBar(ImGui::BeginMenuBar(), &ImGui::EndMenuBar) {}

MainMenu::MainMenu() : MenuBar(ImGui::BeginMainMenuBar(), &ImGui::EndMainMenuBar) {}

Popup::Popup(char const* id, Bool modal, Bool closeable, Bool centered, int flags) : Canvas(false, &ImGui::EndPopup) {
	if (centered) {
		auto const center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});
	}
	if (modal) {
		m_open = ImGui::BeginPopupModal(id, &closeable.value, flags);
	} else {
		m_open = ImGui::BeginPopup(id, flags);
	}
}

void Popup::open(char const* id) { ImGui::OpenPopup(id); }

void Popup::close_current() { ImGui::CloseCurrentPopup(); }

Menu::Menu(NotClosed<MenuBar>, char const* label, Bool enabled) : Openable(ImGui::BeginMenu(label, enabled.value), &ImGui::EndMenu) {}

void StyleVar::push(int index, glm::vec2 value) {
	ImGui::PushStyleVar(index, {value.x, value.y});
	++m_count;
}

void StyleVar::push(int index, float value) {
	ImGui::PushStyleVar(index, value);
	++m_count;
}

StyleVar::~StyleVar() { ImGui::PopStyleVar(m_count); }

TabBar::TabBar(char const* label, int flags) : Openable(ImGui::BeginTabBar(label, flags), &ImGui::EndTabBar) {}

TabBar::Item::Item(NotClosed<TabBar>, char const* label, bool* open, int flags) : Openable(ImGui::BeginTabItem(label, open, flags), &ImGui::EndTabItem) {}

Combo::Combo(char const* label, char const* preview) : Openable(ImGui::BeginCombo(label, preview), &ImGui::EndCombo) {}
} // namespace levk::editor
