#include <imgui.h>
#include <levk/asset/common.hpp>
#include <levk/engine.hpp>
#include <levk/imcpp/reflector.hpp>
#include <levk/scene/entity.hpp>
#include <levk/scene/freecam_controller.hpp>
#include <levk/service.hpp>
#include <levk/window/window.hpp>

namespace levk {
void FreecamController::tick(Time dt) {
	auto data = owning_entity()->transform().data();
	auto& window = Service<Engine>::locate().window();

	if (input().is_held(MouseButton::eRight)) {
		if (window.cursor_mode() != CursorMode::eDisabled) { window.set_cursor_mode(CursorMode::eDisabled); }
	} else {
		if (window.cursor_mode() == CursorMode::eDisabled) { window.set_cursor_mode(CursorMode::eNormal); }
	}

	auto dxy = glm::vec2{};
	if (window.cursor_mode() != CursorMode::eDisabled) {
		m_prev_cursor = input().cursor;
	} else {
		dxy = input().cursor - m_prev_cursor;
		dxy.y = -dxy.y;
		auto const d_pitch_yaw = look_speed * glm::vec2{glm::radians(dxy.y), glm::radians(dxy.x)};
		pitch.value -= d_pitch_yaw.x;
		yaw.value -= d_pitch_yaw.y;

		auto dxyz = glm::vec3{};
		auto front = data.orientation * front_v;
		auto right = data.orientation * right_v;
		auto up = data.orientation * up_v;

		if (input().is_held(Key::eW) || input().is_held(Key::eUp)) { dxyz.z -= 1.0f; }
		if (input().is_held(Key::eS) || input().is_held(Key::eDown)) { dxyz.z += 1.0f; }
		if (input().is_held(Key::eA) || input().is_held(Key::eLeft)) { dxyz.x -= 1.0f; }
		if (input().is_held(Key::eD) || input().is_held(Key::eRight)) { dxyz.x += 1.0f; }
		if (input().is_held(Key::eQ)) { dxyz.y -= 1.0f; }
		if (input().is_held(Key::eE)) { dxyz.y += 1.0f; }
		if (std::abs(dxyz.x) > 0.0f || std::abs(dxyz.y) > 0.0f || std::abs(dxyz.z) > 0.0f) {
			dxyz = glm::normalize(dxyz);
			auto const factor = dt.count() * move_speed;
			data.position += factor * front * dxyz.z;
			data.position += factor * right * dxyz.x;
			data.position += factor * up * dxyz.y;
		}
	}

	data.orientation = glm::vec3{pitch, yaw, 0.0f};
	m_prev_cursor = input().cursor;

	owning_entity()->transform().set_data(data);
}

bool FreecamController::serialize(dj::Json& out) const {
	asset::to_json(out["move_speed"], move_speed);
	out["look_speed"] = look_speed;
	out["pitch"] = Degrees{pitch}.value;
	out["yaw"] = Degrees{yaw}.value;
	return true;
}

bool FreecamController::deserialize(dj::Json const& json) {
	asset::from_json(json["move_speed"], move_speed, move_speed);
	look_speed = json["look_speed"].as<float>(look_speed);
	pitch = Degrees{json["pitch"].as<float>()};
	yaw = Degrees{json["yaw"].as<float>()};
	return true;
}

void FreecamController::inspect(imcpp::OpenWindow w) {
	auto const reflector = imcpp::Reflector{w};
	reflector("Move Speed", move_speed, 0.25f, 0.0f);
	ImGui::DragFloat("Look Speed", &look_speed, 0.025f, 0.0f, 10.0f);
	reflector("Pitch", pitch, 0.05f, -89.0f, 89.0f);
	reflector("Yaw", yaw, 0.1f);
}
} // namespace levk
