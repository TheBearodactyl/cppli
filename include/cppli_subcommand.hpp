#ifndef CPPLI_SUBCOMMAND_HPP
#define CPPLI_SUBCOMMAND_HPP

#include "cppli_error.hpp"
#include "cppli_types.hpp"
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cli {

	// Forward declaration
	class Parser;

	/**
	 * @brief Represents a subcommand with its own flags, positionals, and nested subcommands.
	 *
	 * Subcommands allow organizing complex CLI applications with multiple modes of operation,
	 * similar to git (git add, git commit) or docker (docker run, docker build).
	 *
	 * Example:
	 * @code
	 * cli::Parser parser("myapp", "Application with subcommands", "1.0.0");
	 * auto& init_cmd = parser.add_subcommand("init", "Initialize a project");
	 * init_cmd.add_flag<std::string>("name", "Project name").set_required();
	 *
	 * auto& build_cmd = parser.add_subcommand("build", "Build the project");
	 * build_cmd.add_flag<bool>("release", "Build in release mode");
	 *
	 * auto result = parser.parse(argc, argv);
	 * if (parser.get_selected_subcommand() == "init") {
	 *     // Handle init subcommand
	 * }
	 * @endcode
	 */
	class Subcommand {
	  public:
		/**
		 * @brief Construct a subcommand.
		 * @param name The name of the subcommand (used on command line).
		 * @param description Brief description for help text.
		 * @param parent Pointer to parent Parser (for accessing global state).
		 * @param parent_subcommand Pointer to parent Subcommand (for nested subcommands).
		 */
		Subcommand(std::string name, std::string description, Parser *parent = nullptr, Subcommand *parent_subcommand = nullptr);

		/**
		 * @brief Add a typed flag to this subcommand.
		 * @tparam T Type of the flag value (default: std::string).
		 * @param long_name Long name without dashes.
		 * @param description Help text.
		 * @return TypedFlag<T>& Reference for chaining configuration.
		 */
		template <typename T = std::string>
		TypedFlag<T> &add_flag(std::string long_name, std::string description);

		/**
		 * @brief Add a typed positional argument to this subcommand.
		 * @tparam T Type of the positional value (default: std::string).
		 * @param name Display name.
		 * @param description Help text.
		 * @param required Whether the positional is mandatory.
		 * @return TypedPositional<T>& Reference for chaining configuration.
		 */
		template <typename T = std::string>
		TypedPositional<T> &add_positional(std::string name, std::string description, bool required = true);

		/**
		 * @brief Add a nested subcommand.
		 * @param name Subcommand name.
		 * @param description Brief description.
		 * @return Subcommand& Reference to the created subcommand.
		 */
		Subcommand &add_subcommand(std::string name, std::string description);

		/**
		 * @brief Set a callback to be invoked when this subcommand is selected.
		 * @param callback Function to execute.
		 * @return Subcommand& for chaining.
		 */
		Subcommand &set_callback(std::function<void()> callback);

		/**
		 * @brief Add help flag to this subcommand.
		 * @return Subcommand& for chaining.
		 */
		Subcommand &add_help_flag();

		/**
		 * @brief Add an example usage for this subcommand.
		 * @param description Example description.
		 * @param command Example command line.
		 * @return Subcommand& for chaining.
		 */
		Subcommand &add_example(std::string description, std::string command);

		/**
		 * @brief Set whether this subcommand allows unknown flags to fallthrough to parent.
		 * @param allow If true, unknown flags are passed to parent for matching.
		 * @return Subcommand& for chaining.
		 */
		Subcommand &set_fallthrough(bool allow = true);

		/**
		 * @brief Get flag value by name.
		 * @tparam T Expected type.
		 * @param flag_name Long name of the flag.
		 * @return std::optional<T> Value if present and type matches.
		 */
		template <typename T = std::string>
		[[nodiscard]] std::optional<T> get(std::string_view flag_name) const;

		/**
		 * @brief Check if flag was provided.
		 * @param flag_name Long name of the flag.
		 * @return bool True if flag has a value.
		 */
		[[nodiscard]] bool has(std::string_view flag_name) const;

		/**
		 * @brief Get positional argument by index.
		 * @tparam T Expected type.
		 * @param index Zero-based index.
		 * @return std::optional<T> Value if present and type matches.
		 */
		template <typename T = std::string>
		[[nodiscard]] std::optional<T> get_positional(size_t index) const;

		/**
		 * @brief Get positional argument by name.
		 * @tparam T Expected type.
		 * @param name Display name of the positional.
		 * @return std::optional<T> Value if present and type matches.
		 */
		template <typename T = std::string>
		[[nodiscard]] std::optional<T> get_positional(std::string_view name) const;

		/**
		 * @brief Get the selected nested subcommand name, if any.
		 * @return std::optional<std::string> Name if a subcommand was parsed.
		 */
		[[nodiscard]] std::optional<std::string> get_selected_subcommand() const;

		/**
		 * @brief Access a nested subcommand by name.
		 * @param name Subcommand name.
		 * @return Subcommand* Pointer to subcommand, or nullptr if not found.
		 */
		[[nodiscard]] Subcommand *get_subcommand(std::string_view name);

		/**
		 * @brief Check if this subcommand was selected during parsing.
		 * @return bool True if parsed.
		 */
		[[nodiscard]] bool parsed() const noexcept {
			return parsed_;
		}

		/**
		 * @brief Get the name of this subcommand.
		 * @return const std::string& Name.
		 */
		[[nodiscard]] const std::string &name() const noexcept {
			return name_;
		}

		/**
		 * @brief Get the description of this subcommand.
		 * @return const std::string& Description.
		 */
		[[nodiscard]] const std::string &description() const noexcept {
			return description_;
		}

		/**
		 * @brief Generate help text specific to this subcommand.
		 * @param full_chain If true, include parent command names.
		 * @return std::string Formatted help output.
		 */
		[[nodiscard]] std::string generate_help(bool full_chain = true) const;

		/**
		 * @brief Print help to an output stream.
		 * @param os Output stream (default: std::cout).
		 * @param full_chain Include parent command names.
		 */
		void print_help(std::ostream &os = std::cout, bool full_chain = true) const;

		/**
		 * @brief Invoke the callback if set.
		 */
		void invoke_callback() const;

	  private:
		friend class Parser;

		/**
		 * @brief Type-erased flag storage (same as in Parser).
		 */
		struct FlagStorage {
			std::unique_ptr<void, void (*)(void *)> ptr;
			std::function<Result<void>(std::string_view)> set_value;
			std::function<Result<void>()> validate;
			std::function<bool()> has_value;
			std::function<bool()> is_required;
			std::function<std::string()> get_short_name;
			std::function<std::string()> get_description;
			std::function<std::string()> format_for_help;
			std::function<std::optional<std::string>()> get_value_as_string;
			std::function<bool()> is_boolean;

			FlagStorage()
				: ptr(nullptr, [](void *) {
				  }) {
			}
		};

		/**
		 * @brief Type-erased positional storage (same as in Parser).
		 */
		struct PositionalStorage {
			std::unique_ptr<void, void (*)(void *)> ptr;
			std::function<Result<void>(std::string_view)> set_value;
			std::function<bool()> has_value;
			std::function<bool()> is_required;
			std::function<std::string()> get_name;
			std::function<std::string()> get_description;
			std::function<std::optional<std::string>()> get_value_as_string;

			PositionalStorage()
				: ptr(nullptr, [](void *) {
				  }) {
			}
		};

		/**
		 * @brief Example usage line.
		 */
		struct Example {
			std::string description;
			std::string command;
		};

		std::string name_;
		std::string description_;
		Parser *parent_;
		Subcommand *parent_subcommand_;
		std::map<std::string, FlagStorage> flags_;
		std::map<std::string, std::string> short_to_long_;
		std::vector<PositionalStorage> positionals_;
		std::map<std::string, std::unique_ptr<Subcommand>> subcommands_;
		std::vector<Example> examples_;
		std::optional<std::string> selected_subcommand_;
		std::function<void()> callback_;
		bool parsed_ = false;
		bool help_requested_ = false;
		bool fallthrough_ = false;

		/**
		 * @brief Parse arguments for this subcommand.
		 * @param args Arguments to parse (starting from first arg after subcommand name).
		 * @param start_index Index to start parsing from.
		 * @return Result<size_t> Number of args consumed, or error.
		 */
		[[nodiscard]] Result<size_t> parse_args(const std::vector<std::string> &args, size_t start_index);

		/**
		 * @brief Validate requirements after parsing.
		 * @return Result<void> ok() if valid, err() otherwise.
		 */
		[[nodiscard]] Result<void> validate_requirements() const;

		/**
		 * @brief Format a flag for help output.
		 * @param flag The flag storage.
		 * @param long_name Flag's long name.
		 * @return std::string Formatted line.
		 */
		[[nodiscard]] std::string format_flag_for_help(const FlagStorage &flag, const std::string &long_name) const;

		/**
		 * @brief Get the full command chain (parent commands + this command).
		 * @return std::string Full command path.
		 */
		[[nodiscard]] std::string get_command_chain() const;
	};

	// Template implementations
	template <typename T>
	TypedFlag<T> &Subcommand::add_flag(std::string long_name, std::string description) {
		auto flag = std::make_unique<TypedFlag<T>>(long_name, std::move(description));
		auto *flag_ptr = flag.get();

		FlagStorage storage;
		storage.ptr = std::unique_ptr<void, void (*)(void *)>(flag.release(), [](void *p) {
			delete static_cast<TypedFlag<T> *>(p);
		});
		storage.set_value = [flag_ptr](std::string_view str) {
			return flag_ptr->set_value_from_string(str);
		};
		storage.validate = [flag_ptr]() {
			return flag_ptr->validate();
		};
		storage.has_value = [flag_ptr]() {
			return flag_ptr->has_value();
		};
		storage.is_required = [flag_ptr]() {
			return flag_ptr->is_required();
		};
		storage.get_short_name = [flag_ptr]() {
			return flag_ptr->short_name();
		};
		storage.get_description = [flag_ptr]() {
			return flag_ptr->description();
		};
		storage.format_for_help = [this, long_name = std::string(long_name)]() {
			return this->format_flag_for_help(flags_.at(long_name), long_name);
		};
		storage.get_value_as_string = [flag_ptr]() -> std::optional<std::string> {
			if (flag_ptr->has_value()) {
				if constexpr (std::is_same_v<T, std::string>) {
					return flag_ptr->value();
				} else if constexpr (std::is_same_v<T, bool>) {
					return flag_ptr->value() ? "true" : "false";
				} else {
					return std::to_string(*flag_ptr->value());
				}
			}
			return std::nullopt;
		};
		storage.is_boolean = []() {
			return std::is_same_v<T, bool>;
		};

		auto *result_ptr = storage.ptr.get();
		flags_[long_name] = std::move(storage);
		return *static_cast<TypedFlag<T> *>(result_ptr);
	}

	template <typename T>
	TypedPositional<T> &Subcommand::add_positional(std::string name, std::string description, bool required) {
		auto pos = std::make_unique<TypedPositional<T>>(name, std::move(description), required);
		auto *pos_ptr = pos.get();

		PositionalStorage storage;
		storage.ptr = std::unique_ptr<void, void (*)(void *)>(pos.release(), [](void *p) {
			delete static_cast<TypedPositional<T> *>(p);
		});
		storage.set_value = [pos_ptr](std::string_view str) {
			return pos_ptr->set_value_from_string(str);
		};
		storage.has_value = [pos_ptr]() {
			return pos_ptr->has_value();
		};
		storage.is_required = [pos_ptr]() {
			return pos_ptr->is_required();
		};
		storage.get_name = [pos_ptr]() {
			return pos_ptr->name();
		};
		storage.get_description = [pos_ptr]() {
			return pos_ptr->description();
		};
		storage.get_value_as_string = [pos_ptr]() -> std::optional<std::string> {
			if (pos_ptr->has_value()) {
				if constexpr (std::is_same_v<T, std::string>) {
					return pos_ptr->value();
				} else if constexpr (std::is_same_v<T, bool>) {
					return pos_ptr->value() ? "true" : "false";
				} else {
					return std::to_string(*pos_ptr->value());
				}
			}
			return std::nullopt;
		};

		positionals_.push_back(std::move(storage));
		return *static_cast<TypedPositional<T> *>(positionals_.back().ptr.get());
	}

	template <typename T>
	std::optional<T> Subcommand::get(std::string_view flag_name) const {
		auto it = flags_.find(std::string(flag_name));
		if (it == flags_.end() || ! it->second.has_value()) {
			return std::nullopt;
		}

		auto *flag_ptr = static_cast<TypedFlag<T> *>(it->second.ptr.get());
		if (! flag_ptr->has_value()) {
			return std::nullopt;
		}

		return flag_ptr->value();
	}

	template <typename T>
	std::optional<T> Subcommand::get_positional(size_t index) const {
		if (index >= positionals_.size()) {
			return std::nullopt;
		}

		auto *pos_ptr = static_cast<TypedPositional<T> *>(positionals_[index].ptr.get());
		if (! pos_ptr->has_value()) {
			return std::nullopt;
		}

		return pos_ptr->value();
	}

	template <typename T>
	std::optional<T> Subcommand::get_positional(std::string_view name) const {
		for (const auto &pos_storage: positionals_) {
			if (pos_storage.get_name() == name) {
				auto *pos_ptr = static_cast<TypedPositional<T> *>(pos_storage.ptr.get());
				if (! pos_ptr->has_value()) {
					return std::nullopt;
				}
				return pos_ptr->value();
			}
		}
		return std::nullopt;
	}
}// namespace cli

#endif// CPPLI_SUBCOMMAND_HPP
