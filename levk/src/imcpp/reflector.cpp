#include <imgui.h>
#include <levk/imcpp/reflector.hpp>
#include <levk/util/fixed_string.hpp>
#include <optional>

namespace levk::imcpp {
namespace {
constexpr glm::vec3 to_degree(glm::vec3 const& angles) { return {glm::degrees(angles.x), glm::degrees(angles.y), glm::degrees(angles.z)}; }

struct Modified {
	bool value{};

	constexpr bool operator()(bool const modified) {
		value |= modified;
		return modified;
	}
};

using DragFloatFunc = bool (*)(char const*, float* v, float, float, float, char const*, ImGuiSliderFlags);

constexpr DragFloatFunc drag_float_vtable[] = {
	nullptr, &ImGui::DragFloat, &ImGui::DragFloat2, &ImGui::DragFloat3, &ImGui::DragFloat4,
};

template <std::size_t Dim>
bool drag_float(char const* label, float (&data)[Dim], float speed, float lo, float hi, int flags = {}) {
	static_assert(Dim > 0 && Dim < std::size(drag_float_vtable));
	return drag_float_vtable[Dim](label, data, speed, lo, hi, "%.3f", flags);
}

template <std::size_t Dim>
bool reflect_vec(char const* label, glm::vec<static_cast<int>(Dim), float>& out_vec, float speed, float lo, float hi) {
	float data[Dim]{};
	std::memcpy(data, &out_vec, Dim * sizeof(float));
	if (drag_float(label, data, speed, lo, hi)) {
		std::memcpy(&out_vec, data, Dim * sizeof(float));
		return true;
	}
	return false;
}
} // namespace

bool Reflector::operator()(char const* label, glm::vec2& out_vec2, float speed, float lo, float hi) const {
	return reflect_vec<2>(label, out_vec2, speed, lo, hi);
}

bool Reflector::operator()(char const* label, glm::vec3& out_vec3, float speed, float lo, float hi) const {
	return reflect_vec<3>(label, out_vec3, speed, lo, hi);
}

bool Reflector::operator()(char const* label, glm::vec4& out_vec4, float speed, float lo, float hi) const {
	return reflect_vec<4>(label, out_vec4, speed, lo, hi);
}

bool Reflector::operator()(char const* label, nvec3& out_vec3, float speed) const {
	auto vec3 = out_vec3.value();
	if ((*this)(label, vec3, speed, -1.0f, 1.0f)) {
		out_vec3 = vec3;
		return true;
	}
	return false;
}

bool Reflector::operator()(char const* label, glm::quat& out_quat) const {
	auto euler = to_degree(glm::eulerAngles(out_quat));
	float deg[3] = {euler.x, euler.y, euler.z};
	auto const org = euler;
	bool ret = false;
	if (drag_float(label, deg, 0.5f, -180.0f, 180.0f, ImGuiSliderFlags_NoInput)) {
		euler = {deg[0], deg[1], deg[2]};
		if (auto const diff = org.x - euler.x; std::abs(diff) > 0.0f) { out_quat = glm::rotate(out_quat, glm::radians(diff), right_v); }
		if (auto const diff = org.y - euler.y; std::abs(diff) > 0.0f) { out_quat = glm::rotate(out_quat, glm::radians(diff), up_v); }
		if (auto const diff = org.z - euler.z; std::abs(diff) > 0.0f) { out_quat = glm::rotate(out_quat, glm::radians(diff), front_v); }
		ret = true;
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Reset")) {
		out_quat = quat_identity_v;
		ret = true;
	}
	return ret;
}

bool Reflector::operator()(char const* label, Radians& out_as_degrees, float speed, float lo, float hi) const {
	auto deg = Degrees{out_as_degrees};
	if (ImGui::DragFloat(label, &deg.value, speed, lo, hi)) {
		out_as_degrees = deg;
		return true;
	}
	return false;
}

bool Reflector::operator()(char const* label, AsRgb out_rgb) const {
	float arr[3] = {out_rgb.out.x, out_rgb.out.y, out_rgb.out.z};
	if (ImGui::ColorEdit3(label, arr)) {
		out_rgb.out = {arr[0], arr[1], arr[2]};
		return true;
	}
	return false;
}

bool Reflector::operator()(Rgba& out_rgba, Bool show_alpha) const {
	auto hdr = HdrRgba{out_rgba, 1.0f};
	if ((*this)(hdr, show_alpha, {false})) {
		out_rgba = hdr;
		return true;
	}
	return false;
}

bool Reflector::operator()(HdrRgba& out_rgba, Bool show_alpha, Bool show_intensity) const {
	auto ret = Modified{};
	auto vec4 = HdrRgba{out_rgba, 1.0f}.to_vec4();
	auto vec3 = glm::vec3{vec4};
	if ((*this)("RGB", AsRgb{vec3})) {
		out_rgba = HdrRgba::from(glm::vec4{vec3, vec4.w}, out_rgba.intensity);
		ret.value = true;
	}
	if (show_alpha) {
		if (ret(ImGui::DragFloat("Alpha", &vec4.w, 0.05f, 0.0f, 1.0f))) { out_rgba.channels[3] = Rgba::to_u8(vec4.w); }
	}
	if (show_intensity) { ret(ImGui::DragFloat("Intensity", &out_rgba.intensity, 0.05f, 0.1f, 1000.0f)); }
	return ret.value;
}

bool Reflector::operator()(Transform& out_transform, Bool& out_unified_scaling, Bool scaling_toggle) const {
	auto ret = Modified{};
	auto vec3 = out_transform.position();
	if (ret((*this)("Position", vec3, 0.01f))) { out_transform.set_position(vec3); }
	auto quat = out_transform.orientation();
	if (ret((*this)("Orientation", quat))) { out_transform.set_orientation(quat); }
	vec3 = out_transform.scale();
	if (out_unified_scaling) {
		if (ret(ImGui::DragFloat("Scale", &vec3.x, 0.01f))) { out_transform.set_scale({vec3.x, vec3.x, vec3.x}); }
	} else {
		if (ret((*this)("Scale", vec3, 0.01f))) { out_transform.set_scale(vec3); }
	}
	if (scaling_toggle) {
		ImGui::SameLine();
		ImGui::Checkbox("Unified", &out_unified_scaling.value);
	}
	return ret.value;
}

bool Reflector::operator()(Camera& out_camera) const { return ImGui::DragFloat("Exposure", &out_camera.exposure, 0.1f, 0.0f, 10.0f); }

bool Reflector::operator()(UvRect& out_uv) const {
	auto ret = Modified{};
	if (auto tn = TreeNode{"UV Rect"}) {
		ret((*this)("Left-top", out_uv.lt));
		ret((*this)("Right-bottom", out_uv.rb));
	}
	return ret.value;
}
} // namespace levk::imcpp
