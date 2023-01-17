#include <imgui.h>
#include <imgui_internal.h>
#include <levk/imcpp/drag_drop.hpp>

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
} // namespace levk::imcpp
