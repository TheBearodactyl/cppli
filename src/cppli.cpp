#include <cppli.hpp>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <io.h>
#include <stdio.h>
#else
#include <unistd.h>
#endif

namespace cli {

	/**
     * @brief RAII helper class for ANSI color codes
     * 
     * Automatically applies color to output streams when connected to a terminal
     * and resets formatting upon destruction.
     */
	class ColorGuard {
	  public:
		/**
         * @brief Construct a ColorGuard and apply color
         * @param os Output stream to colorize
         * @param color ANSI color code string
         */
		ColorGuard(std::ostream &os, const char *color) : os_(os) {
			if (is_tty())
				os_ << color;
		}

		/**
         * @brief Destructor that resets color formatting
         */
		~ColorGuard() {
			if (is_tty())
				os_ << "\033[0m";
		}

	  private:
		std::ostream &os_;///< Reference to the output stream

		/**
         * @brief Check if stdout is connected to a terminal
         * @returns True if stdout is a TTY, false otherwise
         */
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

	constexpr const char *RESET = "\033[0m";  ///< ANSI reset code
	constexpr const char *BOLD = "\033[1m";	  ///< ANSI bold code
	constexpr const char *GREEN = "\033[32m"; ///< ANSI green color code
	constexpr const char *YELLOW = "\033[33m";///< ANSI yellow color code
	constexpr const char *RED = "\033[31m";	  ///< ANSI red color code

	Parser::Parser(std::string app_name, std::string description, std::string version) : app_name_(std::move(app_name)), description_(std::move(description)), version_(std::move(version)) {
	}

	Parser &Parser::add_help_flag() {
		Flag flag;
		flag.short_name = "h";
		flag.long_name = "help";
		flag.description = "Display this help message";
		flag.action = [this]() {
			print_help();
			std::exit(0);
		};
		flags_["help"] = std::move(flag);
		short_to_long_["h"] = "help";
		return *this;
	}

	Parser &Parser::add_version_flag() {
		Flag flag;
		flag.short_name = "V";
		flag.long_name = "version";
		flag.description = "Display version information";
		flag.action = [this]() {
			print_version();
			std::exit(0);
		};
		flags_["version"] = std::move(flag);
		short_to_long_["V"] = "version";
		return *this;
	}

	Parser &Parser::add_positional(std::string name, std::string description, bool required) {
		Positional pos;
		pos.name = std::move(name);
		pos.description = std::move(description);
		pos.required = required;
		positionals_.push_back(std::move(pos));
		return *this;
	}

	Parser &Parser::add_example(std::string description, std::string command) {
		examples_.push_back({std::move(description), std::move(command)});
		return *this;
	}

	ParseResult Parser::parse(int argc, char **argv) {
		std::vector<std::string> args;
		for (int i = 1; i < argc; ++i) {
			args.emplace_back(argv[i]);
		}
		return parse(args);
	}

	ParseResult Parser::parse(const std::vector<std::string> &args) {
		size_t pos_index = 0;
		bool after_double_dash = false;

		for (size_t i = 0; i < args.size(); ++i) {
			const auto &arg = args[i];

			if (arg == "--") {
				after_double_dash = true;
				continue;
			}

			if (after_double_dash || (arg[0] != '-')) {
				if (pos_index >= positionals_.size()) {
					return ParseResult::err("Too many positional arguments");
				}
				positionals_[pos_index++].value = arg;
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
				flag_name = arg.substr(1);
				auto it = short_to_long_.find(flag_name);
				if (it != short_to_long_.end()) {
					flag_name = it->second;
				}
			}

			auto flag_it = flags_.find(flag_name);
			if (flag_it == flags_.end()) {
				return ParseResult::err("Unknown flag: " + arg);
			}

			auto &flag = flag_it->second;

			if (flag.action) {
				flag.action();
				return ParseResult::ok();
			}

			if (! has_value && i + 1 < args.size() && ! args[i + 1].starts_with("-")) {
				flag_value = args[++i];
				has_value = true;
			}

			flag.has_value = true;
			flag.value = has_value ? flag_value : "true";

			if (! flag.validate()) {
				return ParseResult::err("Invalid value for --" + flag_name + ": " + flag.value);
			}
		}

		return validate_requirements();
	}

	ParseResult Parser::validate_requirements() {
		for (const auto &[name, flag]: flags_) {
			if (flag.required && ! flag.has_value) {
				return ParseResult::err("Required flag missing: --" + name);
			}
		}

		for (const auto &pos: positionals_) {
			if (pos.required && pos.value.empty()) {
				return ParseResult::err("Required positional missing: " + pos.name);
			}
		}

		parsed_ = true;
		return ParseResult::ok();
	}

	bool Parser::has(std::string_view flag_name) const {
		auto it = flags_.find(std::string(flag_name));
		return it != flags_.end() && it->second.has_value;
	}

	std::optional<std::string> Parser::get_positional(size_t index) const {
		if (index < positionals_.size() && ! positionals_[index].value.empty()) {
			return positionals_[index].value;
		}
		return std::nullopt;
	}

	std::optional<std::string> Parser::get_positional(std::string_view name) const {
		for (const auto &pos: positionals_) {
			if (pos.name == name && ! pos.value.empty()) {
				return pos.value;
			}
		}
		return std::nullopt;
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

		oss << "\n";
		oss << "USAGE:\n";
		oss << "    " << app_name_ << " [OPTIONS]";

		for (const auto &pos: positionals_) {
			if (pos.required) {
				oss << " <" << pos.name << ">";
			} else {
				oss << " [" << pos.name << "]";
			}
		}
		oss << "\n\n";

		if (! flags_.empty()) {
			oss << "OPTIONS:\n";
			for (const auto &[name, flag]: flags_) {
				oss << "    ";
				if (! flag.short_name.empty()) {
					ColorGuard guard(oss, GREEN);
					oss << "-" << flag.short_name << ", ";
				} else {
					oss << "    ";
				}

				{
					ColorGuard guard(oss, GREEN);
					oss << "--" << flag.long_name;
				}

				if (! flag.default_value.empty()) {
					oss << " [default: " << flag.default_value << "]";
				}
				if (flag.required) {
					ColorGuard guard(oss, RED);
					oss << " (required)";
				}

				oss << "\n";

				if (! flag.description.empty()) {
					oss << "        " << flag.description << "\n";
				}

				if (! flag.choices.empty()) {
					oss << "        Choices: ";
					for (size_t i = 0; i < flag.choices.size(); ++i) {
						if (i > 0)
							oss << ", ";
						oss << flag.choices[i];
					}
					oss << "\n";
				}
			}
			oss << "\n";
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

	FlagBuilder::FlagBuilder(Parser *parser, const std::string &long_name) : parser_(parser), long_name_(long_name) {
	}

	FlagBuilder &FlagBuilder::short_name(char name) {
		return short_name(std::string(1, name));
	}

	FlagBuilder &FlagBuilder::short_name(const std::string &name) {
		auto &flag = parser_->flags_[long_name_];
		flag.short_name = name;
		parser_->short_to_long_[name] = long_name_;
		return *this;
	}

	FlagBuilder &FlagBuilder::long_desc(const std::string &desc) {
		parser_->flags_[long_name_].long_description = desc;
		return *this;
	}

	FlagBuilder &FlagBuilder::required(bool req) {
		parser_->flags_[long_name_].required = req;
		return *this;
	}

	FlagBuilder &FlagBuilder::default_value(const std::string &val) {
		auto &flag = parser_->flags_[long_name_];
		flag.default_value = val;
		flag.value = val;
		flag.has_value = true;
		return *this;
	}

	FlagBuilder &FlagBuilder::choices(const std::vector<std::string> &options) {
		parser_->flags_[long_name_].choices = options;
		return *this;
	}

	FlagBuilder &FlagBuilder::validator(std::function<bool(const std::string &)> fn) {
		parser_->flags_[long_name_].validator = std::move(fn);
		return *this;
	}

	FlagBuilder &FlagBuilder::action(std::function<void()> fn) {
		parser_->flags_[long_name_].action = std::move(fn);
		return *this;
	}

	FlagBuilder &Parser::add_flag(std::string long_name, std::string description) {
		if (flags_.find(long_name) == flags_.end()) {
			Flag flag;
			flag.long_name = long_name;
			flag.description = std::move(description);
			flags_[long_name] = std::move(flag);
		}

		flag_builders_.emplace_back(std::make_unique<FlagBuilder>(this, long_name));
		return *flag_builders_.back();
	}
}// namespace cli
