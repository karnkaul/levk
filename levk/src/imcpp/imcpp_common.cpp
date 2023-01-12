#include <imgui.h>
#include <imgui_internal.h>
#include <levk/imcpp/common.hpp>
#include <cassert>

namespace levk::imcpp {
namespace {
DragDrop::Payload from(ImGuiPayload const& payload) {
	return DragDrop::Payload{
		payload.DataType,
		payload.Data,
		static_cast<std::size_t>(payload.DataSize),
	};
}
} // namespace

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

bool input_text(char const* label, char* buffer, std::size_t size, int flags) { return ImGui::InputText(label, buffer, size, flags); }

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

DragDrop::Source::Source(int flags) : Openable(ImGui::BeginDragDropSource(flags), &ImGui::EndDragDropSource) {}

bool DragDrop::Source::is_active() { return ImGui::IsDragDropActive(); }
bool DragDrop::Source::is_being_accepted() { return ImGui::IsDragDropPayloadBeingAccepted(); }

DragDrop::Target::Target() : Openable(ImGui::BeginDragDropTarget(), &ImGui::EndDragDropTarget) {}

void DragDrop::set(Payload payload, std::string_view label, int cond) {
	ImGui::SetDragDropPayload(payload.type_name, payload.data, payload.size, cond);
	ImGui::Text("%.*s", static_cast<int>(label.size()), label.data());
}

DragDrop::Payload DragDrop::get() {
	auto const* ret = ImGui::GetDragDropPayload();
	if (!ret) { return {}; }
	return from(*ret);
}

DragDrop::Payload DragDrop::accept(char const* type_name) {
	if (auto const* payload = ImGui::AcceptDragDropPayload(type_name)) { return from(*payload); }
	return {};
}

void DragDrop::set_string(char const* type_name, std::string_view str, std::string_view label, int cond) {
	if (label.empty()) { label = str; }
	set(Payload{type_name, str.data(), str.size()}, label, cond);
}

std::string_view DragDrop::accept_string(char const* type_name) {
	auto ret = accept(type_name);
	if (!ret) { return {}; }
	return std::string_view{static_cast<char const*>(ret.data), ret.size};
}

ListBox::ListBox(char const* label, glm::vec2 size) : Openable(ImGui::BeginListBox(label, {size.x, size.y}), &ImGui::EndListBox) {}
} // namespace levk::imcpp
