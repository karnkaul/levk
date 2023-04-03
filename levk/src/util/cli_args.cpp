#include <fmt/format.h>
#include <levk/util/cli_args.hpp>
#include <levk/util/visitor.hpp>
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <span>
#include <variant>

namespace levk {
namespace {
cli_args::Opt const* find_opt(cli_args::Spec const& spec, std::variant<std::string_view, char> key) {
	for (auto const& opt : spec.options) {
		auto const visitor = Visitor{
			[opt](std::string_view const full) { return opt.key.full == full; },
			[opt](char const single) { return opt.key.single == single; },
		};
		if (std::visit(visitor, key)) { return &opt; }
	}
	return {};
}

struct OptParser {
	using Result = cli_args::Result;

	cli_args::Spec valid_spec{};
	Ptr<cli_args::Parser> out;

	OptParser(cli_args::Spec spec, Ptr<cli_args::Parser> out) : out(out) {
		valid_spec.app_name = spec.app_name;
		valid_spec.commands = std::move(spec.commands);
		valid_spec.version = spec.version.empty() ? "(unknown)" : spec.version;
		std::erase_if(spec.options, [](cli_args::Opt const& o) { return !o.key.valid(); });
		valid_spec.options.reserve(spec.options.size() + 2);
		std::move(spec.options.begin(), spec.options.end(), std::back_inserter(valid_spec.options));
		valid_spec.options.push_back(cli_args::Opt{.key = {.full = "help"}, .help = "Show this help text"});
		valid_spec.options.push_back(cli_args::Opt{.key = {.full = "version"}, .help = "Display the version"});
	}

	std::size_t get_max_width() const {
		auto ret = std::size_t{};
		for (auto const& opt : valid_spec.options) {
			auto width = opt.key.full.size();
			if (!opt.value.empty()) {
				width += 1; // =
				if (opt.is_optional_value) {
					width += 2; // []
				}
				width += opt.value.size();
			}
			ret = std::max(ret, width);
		}
		return ret;
	}

	Result print_help() const {
		auto str = fmt::format("Usage: {} [OPTION]...", valid_spec.app_name);
		str.reserve(1024);
		if (!valid_spec.commands.empty()) { fmt::format_to(std::back_inserter(str), " <COMMAND> [OPTION]..."); }
		fmt::format_to(std::back_inserter(str), " {}\n\n", valid_spec.arguments);
		auto const max_width = get_max_width();
		for (auto const& opt : valid_spec.options) {
			fmt::format_to(std::back_inserter(str), "  {}{}", (opt.key.single ? '-' : ' '), (opt.key.single ? opt.key.single : ' '));
			if (!opt.key.full.empty()) { fmt::format_to(std::back_inserter(str), "{} --{}", (opt.key.single ? ',' : ' '), opt.key.full); }
			auto width = opt.key.full.size();
			if (!opt.value.empty()) {
				fmt::format_to(std::back_inserter(str), "{}={}{}", (opt.is_optional_value ? "[" : ""), opt.value, (opt.is_optional_value ? "]" : ""));
				width += (opt.is_optional_value ? 3 : 1) + opt.value.size();
			}
			assert(width <= max_width);
			auto const remain = max_width - width + 4;
			for (std::size_t i = 0; i < remain; ++i) { str += ' '; }
			fmt::format_to(std::back_inserter(str), "{}\n", opt.help);
		}
		if (!valid_spec.commands.empty()) {
			fmt::format_to(std::back_inserter(str), "\nCOMMANDS: ");
			bool first{true};
			for (auto const command : valid_spec.commands) {
				if (!first) { fmt::format_to(std::back_inserter(str), " | "); }
				fmt::format_to(std::back_inserter(str), "{}", command);
				first = false;
			}
			fmt::format_to(std::back_inserter(str), "\n");
		}
		std::cout << str << '\n';
		return Result::eExitSuccess;
	}

	Result print_version() const {
		std::cout << fmt::format("{} version {}\n", valid_spec.app_name, valid_spec.version);
		return Result::eExitSuccess;
	}

	cli_args::Result opt(cli_args::Key key, cli_args::Value value) const {
		if (key.full == "help") { return print_help(); }
		if (key.full == "version") { return print_version(); }
		if (out && !out->option(key, value)) { return Result::eExitFailure; }
		return Result::eContinue;
	}
};

struct ParseOpt {
	using Result = cli_args::Result;

	OptParser const& out;

	std::string_view current{};

	static constexpr bool is_option(std::string_view arg) { return arg[0] == '-'; }

	bool at_end() const { return current.empty(); }

	char advance() {
		if (at_end()) { return {}; }
		auto ret = current[0];
		current = current.substr(1);
		return ret;
	}

	char peek() const { return current[0]; }

	Result unknown_option(std::string_view opt) const {
		std::cerr << fmt::format("unknown option: {}\n", opt);
		return Result::eExitFailure;
	}

	Result missing_value(cli_args::Opt const& opt) const {
		auto str = opt.key.full;
		if (str.empty()) { str = {&opt.key.single, 1}; }
		std::cerr << fmt::format("missing required value for option: {}{}\n", (opt.key.full.empty() ? "-" : "--"), str);
		return Result::eExitFailure;
	}

	Result parse_word() {
		auto const it = current.find('=');
		cli_args::Opt const* opt{};
		auto value = cli_args::Value{};
		if (it != std::string_view::npos) {
			auto const key = current.substr(0, it);
			opt = find_opt(out.valid_spec, key);
			if (!opt) { return unknown_option(key); }
			value = current.substr(it + 1);
		} else {
			opt = find_opt(out.valid_spec, current);
			if (!opt) { return unknown_option(current); }
		}
		if (!opt->value.empty() && !opt->is_optional_value && value.empty()) { return missing_value(*opt); }
		return out.opt(opt->key, value);
	}

	Result parse_singles() {
		char prev{};
		while (!at_end()) {
			prev = advance();
			if (peek() == '=') { break; }
			auto const* opt = find_opt(out.valid_spec, prev);
			if (!opt) { return unknown_option({&prev, 1}); }
			return out.opt(opt->key, {});
		}
		if (peek() == '=') {
			if (prev == '\0') { return unknown_option(""); }
			advance();
			auto const* opt = find_opt(out.valid_spec, prev);
			if (!opt) { return unknown_option({&prev, 1}); }
			return out.opt(opt->key, current);
		}
		return Result::eContinue;
	}

	Result parse() {
		assert(is_option(current));
		advance();
		if (peek() == '-') {
			advance();
			return parse_word();
		}
		return parse_singles();
	}

	Result operator()(std::string_view opt_arg) {
		current = opt_arg;
		return parse();
	}
};
} // namespace

auto cli_args::parse(Spec spec, Ptr<Parser> out, std::span<char const* const> args) -> Result {
	if (spec.app_name.empty()) { spec.app_name = args[0]; }
	auto opt_parser = OptParser{std::move(spec), out};
	auto parse_opt = ParseOpt{.out = opt_parser};
	for (; !args.empty(); args = args.subspan(1)) {
		auto const arg = std::string_view{args.front()};
		if (parse_opt.is_option(arg)) {
			if (auto const result = parse_opt(arg); result != Result::eContinue) { return result; }
		} else if (out && out->command.empty() && !opt_parser.valid_spec.commands.empty()) {
			auto it = std::ranges::find(opt_parser.valid_spec.commands, arg);
			if (it == opt_parser.valid_spec.commands.end()) {
				std::cerr << fmt::format("unrecognized command: {}\n", arg);
				return Result::eExitFailure;
			}
			out->command = *it;
		} else {
			break;
		}
	}
	if (out && !out->arguments(args)) { return Result::eExitFailure; }
	return Result::eContinue;
}

auto cli_args::parse(std::string_view version, std::span<char const* const> args) -> Result {
	auto spec = Spec{.version = version};
	return parse(std::move(spec), {}, args);
}
} // namespace levk
