#include <cppli.hpp>
#include <cppli_subcommand.hpp>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <io.h>
#include <stdio.h>
#else
#include <unistd.h>
#endif

namespace cli {

	namespace {
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

	Subcommand::Subcommand(std::string name, std::string description, Parser *parent, Subcommand *parent_subcommand)
		: name_(std::move(name)), description_(std::move(description)), parent_(parent),
		  parent_subcommand_(parent_subcommand) {
	}

	Subcommand &Subcommand::add_subcommand(std::string name, std::string description) {
		auto subcmd = std::make_unique<Subcommand>(name, std::move(description), parent_, this);
		auto *ptr = subcmd.get();
		subcommands_[name] = std::move(subcmd);
		return *ptr;
	}

	Subcommand &Subcommand::set_callback(std::function<void()> callback) {
		callback_ = std::move(callback);
		return *this;
	}

	Subcommand &Subcommand::add_help_flag() {
		add_flag<bool>("help", "Display help for this subcommand").set_short_name("h");
		return *this;
	}

	Subcommand &Subcommand::add_example(std::string description, std::string command) {
		examples_.push_back({std::move(description), std::move(command)});
		return *this;
	}

	Subcommand &Subcommand::set_fallthrough(bool allow) {
		fallthrough_ = allow;
		return *this;
	}

	bool Subcommand::has(std::string_view flag_name) const {
		auto it = flags_.find(std::string(flag_name));
		return it != flags_.end() && it->second.has_value();
	}

	std::optional<std::string> Subcommand::get_selected_subcommand() const {
		return selected_subcommand_;
	}

	Subcommand *Subcommand::get_subcommand(std::string_view name) {
		auto it = subcommands_.find(std::string(name));
		return it != subcommands_.end() ? it->second.get() : nullptr;
	}

	Result<size_t> Subcommand::parse_args(const std::vector<std::string> &args, size_t start_index) {
		short_to_long_.clear();
		for (const auto &[name, storage]: flags_) {
			const auto s_name = storage.get_short_name();
			if (! s_name.empty()) {
				short_to_long_[s_name] = name;
			}
		}

		size_t pos_index = 0;
		bool after_double_dash = false;
		size_t i = start_index;

		while (i < args.size()) {
			const auto &arg = args[i];

			if (arg == "--") {
				after_double_dash = true;
				++i;
				continue;
			}

			if (! after_double_dash && ! arg.empty() && arg[0] != '-') {
				auto sub_it = subcommands_.find(arg);
				if (sub_it != subcommands_.end()) {
					selected_subcommand_ = arg;
					auto &subcommand = *sub_it->second;

					auto result = subcommand.parse_args(args, i + 1);
					if (! result) {
						return Result<size_t>::err(result.error());
					}

					subcommand.parsed_ = true;
					if (subcommand.help_requested_) {
						help_requested_ = true;
					}

					return Result<size_t>::ok(result.value());
				}
			}

			if (after_double_dash || (arg.empty() || arg[0] != '-')) {
				if (pos_index >= positionals_.size()) {
					return Result<size_t>::ok(i);
				}

				auto result = positionals_[pos_index].set_value(arg);
				if (! result) {
					return Result<size_t>::err(result.error());
				}
				++pos_index;
				++i;
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
					if (fallthrough_) {
						// Unknown flag with fallthrough - let parent handle it
						return Result<size_t>::ok(i);
					}
					return Result<size_t>::err(Error::unknown_flag(arg));
				}
			}

			if (flag_name == "help") {
				help_requested_ = true;
			}

			auto flag_it = flags_.find(flag_name);
			if (flag_it == flags_.end()) {
				if (fallthrough_) {
					return Result<size_t>::ok(i);
				}
				return Result<size_t>::err(Error::unknown_flag(arg));
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
				return Result<size_t>::err(Error::missing_flag_value(flag_name));
			}

			auto result = flag_it->second.set_value(flag_value);
			if (! result) {
				return Result<size_t>::err(result.error());
			}

			++i;
		}

		return Result<size_t>::ok(i);
	}

	Result<void> Subcommand::validate_requirements() const {
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

	std::string Subcommand::format_flag_for_help(const FlagStorage &flag, const std::string &long_name) const {
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

	std::string Subcommand::get_command_chain() const {
		std::vector<std::string> chain;

		const Subcommand *current = this;
		while (current != nullptr) {
			chain.push_back(current->name_);
			current = current->parent_subcommand_;
		}

		if (parent_ != nullptr) {
			chain.push_back(parent_->app_name());
		}

		std::reverse(chain.begin(), chain.end());

		std::ostringstream oss;

		for (size_t i = 0; i < chain.size(); ++i) {
			if (i > 0) {
				oss << " ";
			}

			oss << chain[i];
		}

		return oss.str();
	}

	std::string Subcommand::generate_help(bool full_chain) const {
		std::ostringstream oss;
		std::string command_name = full_chain ? get_command_chain() : name_;

		{
			ColorGuard guard(oss, BOLD);
			oss << command_name;
		}
		oss << "\n";

		if (! description_.empty()) {
			oss << description_ << "\n";
		}

		oss << "\nUSAGE:\n";
		oss << "    " << name_ << " [OPTIONS]";

		for (const auto &pos: positionals_) {
			if (pos.is_required()) {
				oss << " <" << pos.get_name() << ">";
			} else {
				oss << " [" << pos.get_name() << "]";
			}
		}

		if (! subcommands_.empty()) {
			oss << " [SUBCOMMAND]";
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
				if (! sub->description_.empty()) {
					oss << " - " << sub->description_;
				}
				oss << "\n";
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

	void Subcommand::print_help(std::ostream &os, bool full_chain) const {
		os << generate_help(full_chain);
	}

	void Subcommand::invoke_callback() const {
		if (callback_) {
			callback_();
		}
	}
}// namespace cli
