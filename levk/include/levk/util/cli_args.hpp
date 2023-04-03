#pragma once
#include <levk/util/ptr.hpp>
#include <span>
#include <string>
#include <vector>

namespace levk::cli_args {
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
	/// \brief Selected command, if any (set by library, to be read by user).
	///
	std::string_view command{};
	///
	/// \brief Callback for a parsed option.
	/// \param key Option key
	/// \param value Value passed (if any)
	///
	virtual bool option(Key key, Value value) = 0;
	///
	/// \brief Callback for arguments remaining after options.
	/// \param args list of verbatim command-line args
	///
	virtual bool arguments(std::span<char const* const> args) = 0;
};

///
/// \brief Specification for application.
///
struct Spec {
	///
	/// \brief Name to print to users.
	///
	std::string_view app_name{};
	///
	/// \brief List of desired options (requires a custom parser).
	///
	/// Invalid keys are removed from the list.
	///
	std::vector<Opt> options{};
	///
	/// \brief List of valid commands, if any.
	///
	/// First arg is parsed as a command if not empty.
	///
	std::vector<std::string_view> commands{};
	///
	/// \brief Text to display for arguments, if any.
	///
	std::string_view arguments{};
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
Result parse(Spec spec, Ptr<Parser> out, std::span<char const* const> args);
///
/// \brief Parse help and version command line args.
/// \param version Application version
/// \param argc Number of command line arguments
/// \param argv Pointer to first argument
///
Result parse(std::string_view version, std::span<char const* const> argv);
} // namespace levk::cli_args
