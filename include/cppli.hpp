#ifndef CLI_HPP
#define CLI_HPP

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/**
 * @file cppli.hpp
 * @brief A modern C++ command-line argument parser library
 * 
 * This library provides a fluent, type-safe interface for parsing command-line
 * arguments with support for flags, positional arguments, validation, and
 * automatic help generation.
 */

/**
 * @namespace cli
 * @brief Main namespace for the command-line parser library
 */
namespace cli {
	/**
	 * @brief Variant type for storing parsed values
	 * 
	 * Can hold string, integer, double, or boolean values.
	 */
	using Value = std::variant<std::string, int, double, bool>;

	/**
	 * @brief Result type for parsing operations
	 * 
	 * Contains information about parsing success or failure, including
	 * error messages and warnings.
	 */
	struct ParseResult {
		bool success;					  ///< Indicates if parsing succeeded
		std::string error;				  ///< Error message if parsing failed
		std::vector<std::string> warnings;///< List of non-fatal warnings

		/**
		 * @brief Conversion operator to bool
		 * @returns True if parsing was successful, false otherwise
		 */
		operator bool() const {
			return success;
		}

		/**
		 * @brief Create a successful parse result
		 * @returns ParseResult indicating success
		 */
		static ParseResult ok() {
			return {true, "", {}};
		}

		/**
		 * @brief Create a failed parse result with error message
		 * @param msg Error message describing the failure
		 * @returns ParseResult indicating failure
		 */
		static ParseResult err(std::string msg) {
			return {false, std::move(msg), {}};
		}
	};

	class Flag;
	class Positional;
	class Parser;

	/**
	 * @brief Builder class for constructing flag configurations
	 * 
	 * Provides a fluent interface for configuring flags with various properties
	 * such as short names, descriptions, default values, and validation rules.
	 */
	class FlagBuilder {
	  public:
		/**
		 * @brief Construct a new FlagBuilder
		 * @param parser Pointer to the owning Parser instance
		 * @param long_name Long name of the flag (without -- prefix)
		 */
		FlagBuilder(Parser *parser, const std::string &long_name);

		/**
		 * @brief Set the short name for the flag (single character)
		 * @param name Single character short name
		 * @returns Reference to this builder for method chaining
		 */
		FlagBuilder &short_name(char name);

		/**
		 * @brief Set the short name for the flag (string form)
		 * @param name Short name as string
		 * @returns Reference to this builder for method chaining
		 */
		FlagBuilder &short_name(const std::string &name);

		/**
		 * @brief Set a detailed description for the flag
		 * @param desc Long description text
		 * @returns Reference to this builder for method chaining
		 */
		FlagBuilder &long_desc(const std::string &desc);

		/**
		 * @brief Mark the flag as required or optional
		 * @param req True if flag is required, false if optional
		 * @returns Reference to this builder for method chaining
		 */
		FlagBuilder &required(bool req = true);

		/**
		 * @brief Set a default value for the flag
		 * @param val Default value as string
		 * @returns Reference to this builder for method chaining
		 */
		FlagBuilder &default_value(const std::string &val);

		/**
		 * @brief Restrict flag values to a specific set of choices
		 * @param options Vector of allowed values
		 * @returns Reference to this builder for method chaining
		 */
		FlagBuilder &choices(const std::vector<std::string> &options);

		/**
		 * @brief Set a custom validation function for the flag
		 * @param fn Function that returns true if value is valid
		 * @returns Reference to this builder for method chaining
		 */
		FlagBuilder &validator(std::function<bool(const std::string &)> fn);

		/**
		 * @brief Set an action to execute when flag is encountered
		 * @param fn Function to execute (typically for help/version flags)
		 * @returns Reference to this builder for method chaining
		 */
		FlagBuilder &action(std::function<void()> fn);

	  private:
		Parser *parser_;	   ///< Pointer to owning parser
		std::string long_name_;///< Long name of the flag being built

		friend class Parser;
	};

	/**
	 * @brief Represents a command-line flag option
	 * 
	 * Contains all configuration and state for a single flag, including
	 * its names, description, value, validation rules, and actions.
	 */
	class Flag {
	  public:
		std::string short_name;							   ///< Short flag name (e.g., "h")
		std::string long_name;							   ///< Long flag name (e.g., "help")
		std::string description;						   ///< Brief description
		std::string long_description;					   ///< Detailed description
		bool required = false;							   ///< Whether flag is required
		bool has_value = false;							   ///< Whether flag has been set
		std::string value;								   ///< Current value of the flag
		std::string default_value;						   ///< Default value if not provided
		std::vector<std::string> choices;				   ///< Valid choices (if restricted)
		std::function<bool(const std::string &)> validator;///< Custom validation function
		std::function<void()> action;					   ///< Action to execute when flag is present

		/**
		 * @brief Validate the current flag value
		 * @returns True if value is valid according to choices/validator
		 */
		bool validate() const {
			if (! choices.empty()) {
				return std::find(choices.begin(), choices.end(), value) != choices.end();
			}

			if (validator) {
				return validator(value);
			}

			return true;
		}
	};

	/**
	 * @brief Represents a positional command-line argument
	 * 
	 * Positional arguments are required or optional arguments that don't use
	 * flag syntax (e.g., filenames, commands).
	 */
	class Positional {
	  public:
		std::string name;								   ///< Name of the positional argument
		std::string description;						   ///< Description of the argument
		bool required = true;							   ///< Whether argument is required
		std::string value;								   ///< Parsed value
		std::function<bool(const std::string &)> validator;///< Custom validation function

		/**
		 * @brief Validate the positional argument value
		 * @returns True if value is valid according to validator
		 */
		bool validate() const {
			if (validator) {
				return validator(value);
			}
			return true;
		}
	};

	/**
	 * @brief Main parser class for command-line arguments
	 * 
	 * Provides methods to define flags, positional arguments, and parse
	 * command-line input. Includes automatic help generation and validation.
	 * 
	 * Example usage:
	 * @code
	 * cli::Parser parser("myapp", "A sample application", "1.0.0");
	 * parser.add_flag("output", "Output file")
	 *       .short_name('o')
	 *       .required();
	 * parser.add_help_flag();
	 * 
	 * auto result = parser.parse(argc, argv);
	 * if (!result) {
	 *     std::cerr << result.error << std::endl;
	 *     return 1;
	 * }
	 * @endcode
	 */
	class Parser {
	  public:
		/**
		 * @brief Construct a new Parser
		 * @param app_name Name of the application
		 * @param description Brief description of the application
		 * @param version Version string (optional)
		 */
		Parser(std::string app_name, std::string description = "", std::string version = "");

		/**
		 * @brief Add a new flag to the parser
		 * @param long_name Long name for the flag (without -- prefix)
		 * @param description Brief description of the flag
		 * @returns Reference to FlagBuilder for further configuration
		 */
		FlagBuilder &add_flag(std::string long_name, std::string description);

		/**
		 * @brief Add a standard help flag (-h, --help)
		 * @returns Reference to this parser for method chaining
		 */
		Parser &add_help_flag();

		/**
		 * @brief Add a standard version flag (-V, --version)
		 * @returns Reference to this parser for method chaining
		 */
		Parser &add_version_flag();

		/**
		 * @brief Add a positional argument
		 * @param name Name of the positional argument
		 * @param description Description of the argument
		 * @param required Whether the argument is required (default: true)
		 * @returns Reference to this parser for method chaining
		 */
		Parser &add_positional(std::string name, std::string description, bool required = true);

		/**
		 * @brief Add a usage example to the help text
		 * @param description Description of what the example demonstrates
		 * @param command The command-line example
		 * @returns Reference to this parser for method chaining
		 */
		Parser &add_example(std::string description, std::string command);

		/**
		 * @brief Parse command-line arguments from C-style argc/argv
		 * @param argc Argument count
		 * @param argv Argument vector
		 * @returns ParseResult indicating success or failure
		 */
		ParseResult parse(int argc, char **argv);

		/**
		 * @brief Parse command-line arguments from a vector of strings
		 * @param args Vector of argument strings
		 * @returns ParseResult indicating success or failure
		 */
		ParseResult parse(const std::vector<std::string> &args);

		/**
		 * @brief Get the value of a flag with type conversion
		 * @tparam T Target type (string, int, double, or bool)
		 * @param flag_name Name of the flag to retrieve
		 * @returns Optional containing the value if present and convertible, nullopt otherwise
		 */
		template <typename T = std::string>
		std::optional<T> get(std::string_view flag_name) const;

		/**
		 * @brief Check if a flag was provided
		 * @param flag_name Name of the flag to check
		 * @returns True if flag was provided, false otherwise
		 */
		bool has(std::string_view flag_name) const;

		/**
		 * @brief Get a positional argument by index
		 * @param index Zero-based index of the positional argument
		 * @returns Optional containing the value if present, nullopt otherwise
		 */
		std::optional<std::string> get_positional(size_t index) const;

		/**
		 * @brief Get a positional argument by name
		 * @param name Name of the positional argument
		 * @returns Optional containing the value if present, nullopt otherwise
		 */
		std::optional<std::string> get_positional(std::string_view name) const;

		/**
		 * @brief Print help text to an output stream
		 * @param os Output stream to write to (default: std::cout)
		 */
		void print_help(std::ostream &os = std::cout) const;

		/**
		 * @brief Print version information to an output stream
		 * @param os Output stream to write to (default: std::cout)
		 */
		void print_version(std::ostream &os = std::cout) const;

		/**
		 * @brief Generate help text as a string
		 * @returns Formatted help text
		 */
		std::string generate_help() const;

	  private:
		std::string app_name_;									 ///< Application name
		std::string description_;								 ///< Application description
		std::string version_;									 ///< Version string
		std::map<std::string, Flag> flags_;						 ///< Map of long names to flags
		std::map<std::string, std::string> short_to_long_;		 ///< Map of short names to long names
		std::vector<Positional> positionals_;					 ///< List of positional arguments
		std::vector<std::unique_ptr<FlagBuilder>> flag_builders_;///< Storage for flag builders

		/**
		 * @brief Internal structure for storing usage examples
		 */
		struct Example {
			std::string description;///< Description of the example
			std::string command;	///< Command-line example text
		};

		std::vector<Example> examples_;///< List of usage examples

		bool parsed_ = false;///< Whether parse() has been called successfully

		/**
		 * @brief Parse a single flag argument
		 * @param arg The flag argument string
		 * @param i Current argument index (may be incremented)
		 * @param argc Total argument count
		 * @param argv Argument vector
		 * @returns ParseResult indicating success or failure
		 */
		ParseResult parse_flag(const std::string &arg, int &i, int argc, char **argv);

		/**
		 * @brief Parse a single positional argument
		 * @param arg The positional argument string
		 * @param pos_index Current positional index (incremented on success)
		 * @returns ParseResult indicating success or failure
		 */
		ParseResult parse_positional(const std::string &arg, size_t &pos_index);

		/**
		 * @brief Validate all requirements after parsing
		 * @returns ParseResult indicating whether all requirements are met
		 */
		ParseResult validate_requirements();

		friend class FlagBuilder;
	};

	template <typename T>
	std::optional<T> Parser::get(std::string_view flag_name) const {
		auto it = flags_.find(std::string(flag_name));
		if (it == flags_.end() || ! it->second.has_value) {
			return std::nullopt;
		}

		const std::string &val = it->second.value;

		if constexpr (std::is_same_v<T, std::string>) {
			return val;
		} else if constexpr (std::is_same_v<T, int>) {
			try {
				return std::stoi(val);
			} catch (...) {
				return std::nullopt;
			}
		} else if constexpr (std::is_same_v<T, double>) {
			try {
				return std::stod(val);
			} catch (...) {
				return std::nullopt;
			}
		} else if constexpr (std::is_same_v<T, bool>) {
			return val == "true" || val == "1" || val == "yes";
		}

		return std::nullopt;
	}
}// namespace cli

#endif
