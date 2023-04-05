#pragma once
#include <levk/frame_profile.hpp>
#include <array>
#include <optional>

namespace levk {
struct FrameProfiler {
	using Type = FrameProfile::Type;

	std::array<FrameProfile, 2> frame_profiles{};
	EnumArray<Type, Clock::time_point> start_map{};
	std::optional<Type> previous_type{};
	std::size_t current_index{};

	void start() { profile(Type::eFrameTime); }

	void profile(Type type) {
		start_map[type] = stop_previous();
		previous_type = type;
	}

	Clock::time_point stop_previous() {
		auto const ret = Clock::now();
		if (previous_type) { frame_profiles[current_index].profile[*previous_type] = ret - start_map[*previous_type]; }
		return ret;
	}

	void finish() {
		frame_profiles[current_index].profile[Type::eFrameTime] = stop_previous() - start_map[Type::eFrameTime];
		current_index = (current_index + 1) % frame_profiles.size();
	}

	FrameProfile previous_profile() const { return frame_profiles[(current_index + 1) % frame_profiles.size()]; }

	static FrameProfiler& instance() {
		static auto ret = FrameProfiler{};
		return ret;
	}
};
} // namespace levk
