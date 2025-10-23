#include <cppli.hpp>
#include <cppli_error.hpp>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <io.h>
#include <stdio.h>
#else
#include <unistd.h>
#endif

namespace cli {

	namespace {
		/**
		 * @brief RAII helper class for ANSI color codes
		 */
		class ColorGuard {
		  public:
			ColorGuard(std::ostream &os, const char *color) : os_(os) {
				if (is_tty())
					os_ << color;
			}

			~ColorGuard() {
				if (is_tty())
					os_ << "\033[0m";
			}

		  private:
			std::ostream &os_;

			static bool is_tty() {
				static bool checked = false;
				static bool is_terminal = false;
				if (! checked) {
#ifdef _WIN32
					is_terminal = _isatty(_fileno(stdout)) != 0;
#else
					is_terminal = isatty(fileno(stdout)) == 1;
#endif
					checked = true;
				}
				return is_terminal;
			}
		};

		constexpr const char *RESET = "\033[0m";
		constexpr const char *BOLD = "\033[1m";
		constexpr const char *GREEN = "\033[32m";
		constexpr const char *YELLOW = "\033[33m";
		constexpr const char *RED = "\033[31m";
	}// namespace

	Parser::Parser(std::string app_name, std::string description, std::string version)
		: app_name_(std::move(app_name)), description_(std::move(description)), version_(std::move(version)) {
	}

	Parser &Parser::add_help_flag() {
		add_flag<bool>("help", "Display this help message").set_short_name("h");
		return *this;
	}

	Parser &Parser::add_version_flag() {
		add_flag<bool>("version", "Display version information").set_short_name("V");
		return *this;
	}

	Parser &Parser::add_example(std::string description, std::string command) {
		examples_.push_back({std::move(description), std::move(command)});
		return *this;
	}

	Subcommand &Parser::add_subcommand(std::string name, std::string description) {
		auto subcommand = std::make_unique<Subcommand>(name, std::move(description), this);
		auto *ptr = subcommand.get();
		subcommands_[name] = std::move(subcommand);
		return *ptr;
	}

	std::optional<std::string> Parser::get_selected_subcommand() const {
		return selected_subcommand_;
	}

	Subcommand *Parser::get_subcommand(std::string_view name) {
		auto it = subcommands_.find(std::string(name));
		return it != subcommands_.end() ? it->second.get() : nullptr;
	}

	Parser &Parser::require_subcommand(int count) {
		required_subcommand_count_ = count;
		return *this;
	}

	Result<void> Parser::parse(int argc, char **argv) {
		std::vector<std::string> args;
		args.reserve(static_cast<size_t>(argc) - 1);
		for (int i = 1; i < argc; ++i) {
			args.emplace_back(argv[i]);
		}
		return parse(args);
	}

	Result<void> Parser::parse(const std::vector<std::string> &args) {
		short_to_long_.clear();
		for (const auto &[name, storage]: flags_) {
			const auto s_name = storage.get_short_name();

			if (! s_name.empty()) {
				short_to_long_[s_name] = name;
			}
		}

		size_t pos_index = 0;
		bool after_double_dash = false;

		for (size_t i = 0; i < args.size(); ++i) {
			const auto &arg = args[i];

			if (arg == "--") {
				after_double_dash = true;
				continue;
			}

			if (! after_double_dash && ! arg.empty() && arg[0] != '-') {
				auto sub_it = subcommands_.find(arg);
				if (sub_it != subcommands_.end()) {
					selected_subcommand_ = arg;
					auto &subcommand = *sub_it->second;

					auto result = subcommand.parse_args(args, i + 1);
					if (! result) {
						return Result<void>::err(result.error());
					}

					subcommand.parsed_ = true;

					if (subcommand.help_requested_) {
						help_requested_ = true;
						parsed_ = true;
						return Result<void>::ok();
					}

					auto validation = subcommand.validate_requirements();
					if (! validation) {
						return validation;
					}

					parsed_ = true;

					subcommand.invoke_callback();

					return Result<void>::ok();
				}
			}

			if (after_double_dash || (arg.empty() || arg[0] != '-')) {
				if (pos_index >= positionals_.size()) {
					return Result<void>::err(Error::too_many_positionals());
				}

				auto result = positionals_[pos_index].set_value(arg);
				if (! result) {
					return result;
				}
				++pos_index;
				continue;
			}

			std::string flag_name;
			std::string flag_value;
			bool has_value = false;

			if (arg.starts_with("--")) {
				size_t eq_pos = arg.find('=');
				if (eq_pos != std::string::npos) {
					flag_name = arg.substr(2, eq_pos - 2);
					flag_value = arg.substr(eq_pos + 1);
					has_value = true;
				} else {
					flag_name = arg.substr(2);
				}
			} else if (arg.starts_with("-")) {
				std::string short_name = arg.substr(1);
				auto it = short_to_long_.find(short_name);
				if (it != short_to_long_.end()) {
					flag_name = it->second;
				} else {
					return Result<void>::err(Error::unknown_flag(arg));
				}
			}

			if (flag_name == "help") {
				help_requested_ = true;
			}

			if (flag_name == "version") {
				version_requested_ = true;
			}

			auto flag_it = flags_.find(flag_name);
			if (flag_it == flags_.end()) {
				return Result<void>::err(Error::unknown_flag(arg));
			}

			bool is_boolean_flag = flag_it->second.is_boolean();

			if (! has_value && i + 1 < args.size() && ! args[i + 1].starts_with("-")) {
				std::string_view next_arg = args[i + 1];

				if (! is_boolean_flag) {
					flag_value = args[++i];
					has_value = true;
				} else {
					if (next_arg == "true" || next_arg == "false" || next_arg == "1" || next_arg == "0" ||
						next_arg == "yes" || next_arg == "no" || next_arg == "on" || next_arg == "off") {
						flag_value = args[++i];
						has_value = true;
					}
				}
			}

			if (! has_value && is_boolean_flag) {
				flag_value = "true";
				has_value = true;
			}

			if (! has_value) {
				return Result<void>::err(Error::missing_flag_value(flag_name));
			}

			auto result = flag_it->second.set_value(flag_value);
			if (! result) {
				return result;
			}
		}

		parsed_ = true;

		if (help_requested_ || version_requested_) {
			return Result<void>::ok();
		}

		if (required_subcommand_count_ != 0) {
			if (required_subcommand_count_ == -1 && ! selected_subcommand_.has_value()) {
				return Result<void>::err(Error(ErrorCode::MissingRequiredFlag, "A subcommand is required"));
			}
		}

		return validate_requirements();
	}

	Result<void> Parser::validate_requirements() const {
		for (const auto &[name, flag]: flags_) {
			if (flag.is_required() && ! flag.has_value()) {
				return Result<void>::err(Error::missing_required_flag(name));
			}
		}

		for (const auto &pos: positionals_) {
			if (pos.is_required() && ! pos.has_value()) {
				return Result<void>::err(Error::missing_required_positional(pos.get_name()));
			}
		}

		return Result<void>::ok();
	}

	bool Parser::has(std::string_view flag_name) const {
		auto it = flags_.find(std::string(flag_name));
		return it != flags_.end() && it->second.has_value();
	}

	void Parser::print_help(std::ostream &os) const {
		os << generate_help();
	}

	void Parser::print_version(std::ostream &os) const {
		ColorGuard guard(os, BOLD);
		os << app_name_;
		if (! version_.empty()) {
			os << " v" << version_;
		}
		os << "\n";
	}

	std::string Parser::format_flag_for_help(const FlagStorage &flag, const std::string &long_name) const {
		std::ostringstream oss;

		oss << "    ";
		std::string short_name = flag.get_short_name();
		if (! short_name.empty()) {
			oss << "-" << short_name << ", ";
		} else {
			oss << "    ";
		}

		oss << "--" << long_name;

		if (flag.is_required()) {
			oss << " (required)";
		}

		return oss.str();
	}

	std::string Parser::generate_help() const {
		std::ostringstream oss;

		{
			ColorGuard guard(oss, BOLD);
			oss << app_name_;
			if (! version_.empty()) {
				oss << " v" << version_;
			}
		}
		oss << "\n";

		if (! description_.empty()) {
			oss << description_ << "\n";
		}

		oss << "\nUSAGE:\n";
		oss << "    " << app_name_ << " [OPTIONS]";

		for (const auto &pos: positionals_) {
			if (pos.is_required()) {
				oss << " <" << pos.get_name() << ">";
			} else {
				oss << " [" << pos.get_name() << "]";
			}
		}

		if (! subcommands_.empty()) {
			if (required_subcommand_count_ != 0) {
				oss << " <SUBCOMMAND>";
			} else {
				oss << " [SUBCOMMAND]";
			}
		}

		oss << "\n\n";

		if (! flags_.empty()) {
			oss << "OPTIONS:\n";
			for (const auto &[name, flag]: flags_) {
				oss << format_flag_for_help(flag, name) << "\n";
				std::string desc = flag.get_description();
				if (! desc.empty()) {
					oss << "        " << desc << "\n";
				}
			}
			oss << "\n";
		}

		if (! subcommands_.empty()) {
			oss << "SUBCOMMANDS:\n";
			for (const auto &[name, sub]: subcommands_) {
				oss << "    " << name;
				if (! sub->description().empty()) {
					oss << " - " << sub->description();
				}
				oss << "\n";
			}
			oss << "\n";
			oss << "Use '" << app_name_ << " <SUBCOMMAND> --help' for more information on a subcommand.\n\n";
		}

		if (! examples_.empty()) {
			oss << "EXAMPLES:\n";
			for (const auto &example: examples_) {
				oss << "  " << example.description << "\n";
				oss << "    $ ";
				ColorGuard guard(oss, GREEN);
				oss << example.command << "\n\n";
			}
		}

		return oss.str();
	}
}// namespace cli
