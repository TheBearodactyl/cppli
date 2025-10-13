#ifndef CPPLI_HPP
#define CPPLI_HPP

#include <cppli_error.hpp>
#include <cppli_types.hpp>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace cli {

	/**
	 * @brief Main parser class for command-line arguments with type safety
	 *
	 * Example usage:
	 * @code
	 * cli::Parser parser("myapp", "A sample application", "1.0.0");
	 * parser.add_flag<int>("port", "Port number").set_required().set_default_value(8080);
	 * parser.add_help_flag();
	 *
	 * auto result = parser.parse(argc, argv);
	 * if (!result) {
	 *     std::cerr << result.error().message() << std::endl;
	 *     return 1;
	 * }
	 * @endcode
	 */
	class Parser {
	  public:
		Parser(std::string app_name, std::string description = "", std::string version = "");

		template <typename T = std::string>
		TypedFlag<T> &add_flag(std::string long_name, std::string description);

		Parser &add_help_flag();
		Parser &add_version_flag();

		template <typename T = std::string>
		TypedPositional<T> &add_positional(std::string name, std::string description, bool required = true);

		Parser &add_example(std::string description, std::string command);

		[[nodiscard]] Result<void> parse(int argc, char **argv);
		[[nodiscard]] Result<void> parse(const std::vector<std::string> &args);

		template <typename T = std::string>
		[[nodiscard]] std::optional<T> get(std::string_view flag_name) const;

		[[nodiscard]] bool has(std::string_view flag_name) const;

		template <typename T = std::string>
		[[nodiscard]] std::optional<T> get_positional(size_t index) const;

		template <typename T = std::string>
		[[nodiscard]] std::optional<T> get_positional(std::string_view name) const;

		void print_help(std::ostream &os = std::cout) const;
		void print_version(std::ostream &os = std::cout) const;
		[[nodiscard]] std::string generate_help() const;

	  private:
		/**
		 * @brief Type-erased container for any TypedFlag<T>.
		 *
		 * Internally stores a unique_ptr to the concrete flag along with function
		 * wrappers to set, validate, and query the flag properties without knowing T.
		 */
		struct FlagStorage {
			std::unique_ptr<void, void (*)(void *)> ptr;			///< owning pointer to TypedFlag<T>
			std::function<Result<void>(std::string_view)> set_value;///< set value from string
			std::function<Result<void>()> validate;					///< run validation
			std::function<bool()> has_value;						///< whether value is present
			std::function<bool()> is_required;						///< whether flag is required
			std::function<std::string()> get_short_name;			///< short name accessor
			std::function<std::string()> get_description;			///< description accessor
			std::function<std::string()> format_for_help;			///< prepared help line formatter

			FlagStorage()
				: ptr(nullptr, [](void *) {
				  }) {
			}
		};

		/**
		 * @brief Type-erased container for any TypedPositional<T>.
		 */
		struct PositionalStorage {
			std::unique_ptr<void, void (*)(void *)> ptr;			///< owning pointer to TypedPositional<T>
			std::function<Result<void>(std::string_view)> set_value;///< set value from string
			std::function<bool()> has_value;						///< whether value is present
			std::function<bool()> is_required;						///< whether positional is required
			std::function<std::string()> get_name;					///< name accessor
			std::function<std::string()> get_description;			///< description accessor

			PositionalStorage()
				: ptr(nullptr, [](void *) {
				  }) {
			}
		};

		/**
		 * @brief A help example line with a description and command.
		 */
		struct Example {
			std::string description;///< Brief explanation of the example.
			std::string command;	///< Shell command shown in the help.
		};

		std::string app_name_;
		std::string description_;
		std::string version_;
		std::map<std::string, FlagStorage> flags_;		  ///< map long-name -> flag
		std::map<std::string, std::string> short_to_long_;///< map short-name -> long-name
		std::vector<PositionalStorage> positionals_;
		std::vector<Example> examples_;
		bool parsed_ = false;			///< true after a successful parse
		bool help_requested_ = false;	///< true if help path was taken
		bool version_requested_ = false;///< true if version path was taken

		/**
		 * @brief Validate required flags and positionals after parsing.
		 * @return Result<void> ok() if all requirements satisfied, err(Error) otherwise.
		 */
		[[nodiscard]] Result<void> validate_requirements() const;

		/**
		 * @brief Format a single flag line for the help output.
		 * @param flag Type-erased flag storage.
		 * @param long_name The long name used for display.
		 * @return std::string Formatted option line.
		 */
		std::string format_flag_for_help(const FlagStorage &flag, const std::string &long_name) const;
	};

	template <typename T>
	TypedFlag<T> &Parser::add_flag(std::string long_name, std::string description) {
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
		storage.format_for_help = [this, flag_ptr, long_name]() {
			return this->format_flag_for_help(flags_.at(long_name), long_name);
		};

		if (! flag_ptr->short_name().empty()) {
			short_to_long_[flag_ptr->short_name()] = long_name;
		}

		flags_[long_name] = std::move(storage);
		return *flag_ptr;
	}

	template <typename T>
	TypedPositional<T> &Parser::add_positional(std::string name, std::string description, bool required) {
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

		positionals_.push_back(std::move(storage));
		return *pos_ptr;
	}

	template <typename T>
	std::optional<T> Parser::get(std::string_view flag_name) const {
		auto it = flags_.find(std::string(flag_name));
		if (it == flags_.end() || ! it->second.has_value()) {
			return std::nullopt;
		}

		auto *flag = static_cast<TypedFlag<T> *>(it->second.ptr.get());
		return flag->value();
	}

	template <typename T>
	std::optional<T> Parser::get_positional(size_t index) const {
		if (index >= positionals_.size()) {
			return std::nullopt;
		}

		auto *pos = static_cast<TypedPositional<T> *>(positionals_[index].ptr.get());
		return pos->value();
	}

	template <typename T>
	std::optional<T> Parser::get_positional(std::string_view name) const {
		for (const auto &pos_storage: positionals_) {
			if (pos_storage.get_name() == name) {
				auto *pos = static_cast<TypedPositional<T> *>(pos_storage.ptr.get());
				return pos->value();
			}
		}
		return std::nullopt;
	}

}// namespace cli

#endif// CPPLI_HPP
