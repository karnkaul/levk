#pragma once
#include <levk/util/ptr.hpp>
#include <string>
#include <vector>

namespace levk {
///
/// \brief Command line options parser.
///
struct CliOpts {
	///
	/// \brief Result of parsing options.
	///
	enum class Result { eContinue, eExitFailure, eExitSuccess };

	///
	/// \brief Specification of an option key.
	///
	struct Key {
		///
		/// \brief Long form of the key.
		///
		std::string_view full{};
		///
		/// \brief Short / character form of the key.
		///
		char single{};

		///
		/// \brief Check if Key is valid.
		/// \returns true if either single is non-null or full is non-empty
		///
		constexpr bool valid() const { return !full.empty() || single != '\0'; }
	};

	///
	/// \brief Value for an option.
	///
	using Value = std::string_view;

	///
	/// \brief Specification for an option consumed by a custom parser.
	///
	struct Opt {
		///
		/// \brief Key specification.
		///
		Key key{};
		///
		/// \brief Value specification (printed in help).
		///
		Value value{};
		///
		/// \brief If value is non-empty, whether it is optional.
		///
		bool is_optional_value{};
		///
		/// \brief Help text for this option.
		///
		std::string_view help{};
	};

	///
	/// \brief Interface for custom parser.
	///
	struct Parser {
		///
		/// \brief Callback for a parsed option.
		/// \param key Option key
		/// \param value Value passed (if any)
		///
		virtual void opt(Key key, Value value) = 0;
	};

	///
	/// \brief Specification for application.
	///
	struct Spec {
		///
		/// \brief List of desired options (requires a custom parser).
		///
		/// Invalid keys are removed from the list.
		///
		std::vector<Opt> options{};
		///
		/// \brief Application version (printed in help).
		///
		std::string_view version{"(unknown)"};
	};

	///
	/// \brief Parse help, version, and custom command line args.
	/// \param spec Application spec
	/// \param out A custom parser (used if custom options are in spec)
	/// \param argc Number of command line arguments
	/// \param argv Pointer to first argument
	///
	static Result parse(Spec spec, Ptr<Parser> out, int argc, char const* const* argv);
	///
	/// \brief Parse help and version command line args.
	/// \param version Application version
	/// \param argc Number of command line arguments
	/// \param argv Pointer to first argument
	///
	static Result parse(std::string_view version, int argc, char const* const* argv);
};
} // namespace levk
