#pragma once
#include <levk/imcpp/common.hpp>
#include <string_view>

namespace levk::imcpp {
template <typename Type>
concept DragDropPayloadT = (!std::is_pointer_v<Type>) && std::is_trivially_copyable_v<Type>;

struct DragDrop {
	class Source : public Openable {
	  public:
		explicit Source(int flags = {});

		static bool is_active();
		static bool is_being_accepted();
	};

	class Target : public Openable {
	  public:
		explicit Target();
	};

	struct Payload {
		char const* type_name{};
		void const* data{};
		std::size_t size{};

		explicit operator bool() const { return type_name && *type_name && data && size; }
	};

	static void set(Payload payload, std::string_view label, int cond = {});
	static Payload get();
	static Payload accept(char const* type_name);

	template <DragDropPayloadT T>
	static void set(char const* type_name, T const& t, std::string_view label, int cond = {}) {
		set(Payload{type_name, &t, sizeof(T)}, label, cond);
	}

	template <DragDropPayloadT T>
	static Ptr<T const> accept(char const* type_name) {
		auto ret = accept(type_name);
		if (!ret || ret.size != sizeof(T)) { return {}; }
		return static_cast<T const*>(ret.data);
	}

	static void set_string(char const* type_name, std::string_view str, std::string_view label = {}, int cond = {});
	static std::string_view accept_string(char const* type_name);
};
} // namespace levk::imcpp
