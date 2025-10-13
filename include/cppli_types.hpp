#ifndef CPPLI_TYPES_HPP
#define CPPLI_TYPES_HPP

#include <cppli_error.hpp>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cli {

	/**
	 * @brief Convert a string_view into a value of type T.
	 *
	 * Specialize this template for your own types to enable parsing those types
	 * from flags or positionals. The function must return Result<T> with either
	 * a parsed value or an Error.
	 *
	 * Built-in specializations provided: std::string, int, double, bool.
	 */
	template <typename T>
	struct ValueConverter {
		/**
		 * @brief Convert string to T.
		 * @param str Source text (unquoted).
		 * @return Result<T> Parsed value or an error.
		 */
		static Result<T> from_string(std::string_view str);
	};

	/**
	 * @brief ValueConverter specialization for std::string (identity).
	 */
	template <>
	struct ValueConverter<std::string> {
		static Result<std::string> from_string(std::string_view str) {
			return Result<std::string>::ok(std::string(str));
		}
	};

	/**
	 * @brief ValueConverter specialization for int.
	 *
	 * Uses std::from_chars for locale-independent parsing.
	 */
	template <>
	struct ValueConverter<int> {
		static Result<int> from_string(std::string_view str);
	};

	/**
	 * @brief ValueConverter specialization for double.
	 *
	 * Uses std::from_chars for locale-independent parsing (C++17 and later).
	 */
	template <>
	struct ValueConverter<double> {
		static Result<double> from_string(std::string_view str);
	};

	/**
	 * @brief ValueConverter specialization for bool.
	 *
	 * Accepted forms (case-sensitive): "true", "false", "1", "0", "yes", "no",
	 * "on", "off".
	 */
	template <>
	struct ValueConverter<bool> {
		static Result<bool> from_string(std::string_view str);
	};

	/**
	 * @brief Validator function signature.
	 *
	 * Return Result<void>::ok() for success, or Result<void>::err(Error(reason)).
	 * You can use Error::validation_failed(name, reason) for convenience.
	 */
	template <typename T>
	using Validator = std::function<Result<void>(const T &)>;

	/**
	 * @brief Strongly-typed flag descriptor with validation and defaults.
	 *
	 * T must have a ValueConverter<T> specialization.
	 *
	 * Example:
	 * @code
	 * auto &flag = parser.add_flag<int>("threads", "Worker thread count");
	 * flag.set_short_name("t")
	 *     .set_required()
	 *     .set_default_value(4)
	 *     .set_validator([](int v) {
	 *       return v > 0 ? cli::Result<void>::ok()
	 *                    : cli::Result<void>::err(cli::Error::validation_failed(
	 *                          "--threads", "must be positive"));
	 *     });
	 * @endcode
	 */
	template <typename T = std::string>
	class TypedFlag {
	  public:
		/**
		 * @brief Construct a TypedFlag with name and description.
		 * @param long_name The long name (without dashes).
		 * @param description Help text for this flag.
		 */
		TypedFlag(std::string long_name, std::string description) : long_name_(std::move(long_name)), description_(std::move(description)) {
		}

		/** @name Accessors */
		///@{
		[[nodiscard]] const std::string &long_name() const noexcept {
			return long_name_;
		}
		[[nodiscard]] const std::string &short_name() const noexcept {
			return short_name_;
		}
		[[nodiscard]] const std::string &description() const noexcept {
			return description_;
		}
		[[nodiscard]] bool is_required() const noexcept {
			return required_;
		}
		[[nodiscard]] bool has_value() const noexcept {
			return value_.has_value();
		}
		[[nodiscard]] const std::optional<T> &value() const noexcept {
			return value_;
		}
		[[nodiscard]] const std::optional<T> &default_value() const noexcept {
			return default_value_;
		}
		[[nodiscard]] const std::vector<T> &choices() const noexcept {
			return choices_;
		}
		///@}

		/**
		 * @brief Assign a short one-letter or compact alias (e.g. "v" for -v).
		 * @param name Short alias without dash.
		 * @return TypedFlag& for chaining.
		 */
		TypedFlag &set_short_name(std::string name) {
			short_name_ = std::move(name);
			return *this;
		}

		/**
		 * @brief Mark the flag as required or not.
		 * @param req True (default true) to require the flag.
		 * @return TypedFlag& for chaining.
		 */
		TypedFlag &set_required(bool req = true) {
			required_ = req;
			return *this;
		}

		/**
		 * @brief Set a default value and initialize the current value to that default.
		 * @param val The default value.
		 * @return TypedFlag& for chaining.
		 */
		TypedFlag &set_default_value(T val) {
			default_value_ = std::move(val);
			value_ = default_value_;
			return *this;
		}

		/**
		 * @brief Restrict the acceptable values to a fixed set.
		 * @param opts Allowed values; validation fails if provided value not in set.
		 * @return TypedFlag& for chaining.
		 */
		TypedFlag &set_choices(std::vector<T> opts) {
			choices_ = std::move(opts);
			return *this;
		}

		/**
		 * @brief Provide a custom validator for additional constraints.
		 * @param fn Function that validates values of type T.
		 * @return TypedFlag& for chaining.
		 */
		TypedFlag &set_validator(Validator<T> fn) {
			validator_ = std::move(fn);
			return *this;
		}

		/**
		 * @brief Parse and set the value from a string, then validate.
		 *
		 * @param str Raw string from the command line.
		 * @return Result<void> ok() if parsed+validated, err(Error) otherwise.
		 */
		Result<void> set_value_from_string(std::string_view str) {
			auto converted = ValueConverter<T>::from_string(str);
			if (! converted) {
				return Result<void>::err(converted.error());
			}

			value_ = std::move(converted.value());
			return validate();
		}

		/**
		 * @brief Run all configured validations.
		 *
		 * Validations include:
		 * - choice membership, if choices are specified
		 * - custom validator, if present
		 *
		 * @return Result<void> ok() if valid, err(Error) otherwise.
		 */
		Result<void> validate() const {
			if (! value_.has_value()) {
				return Result<void>::ok();
			}

			if (! choices_.empty()) {
				bool found = false;
				for (const auto &choice: choices_) {
					if (*value_ == choice) {
						found = true;
						break;
					}
				}
				if (! found) {
					return Result<void>::err(Error::validation_failed(long_name_, "value not in allowed choices"));
				}
			}

			if (validator_) {
				return validator_(*value_);
			}

			return Result<void>::ok();
		}

	  private:
		std::string long_name_;
		std::string short_name_;
		std::string description_;
		bool required_ = false;
		std::optional<T> value_;
		std::optional<T> default_value_;
		std::vector<T> choices_;
		Validator<T> validator_;
	};

	/**
	 * @brief Typed positional argument with optional validation.
	 *
	 * Example:
	 * @code
	 * auto &file = parser.add_positional<std::string>("file", "Path to file");
	 * file.set_validator([](const std::string& v) {
	 *   return v.empty()
	 *     ? cli::Result<void>::err(cli::Error::validation_failed("file", "empty"))
	 *     : cli::Result<void>::ok();
	 * });
	 * @endcode
	 */
	template <typename T = std::string>
	class TypedPositional {
	  public:
		/**
		 * @brief Construct a positional argument.
		 * @param name Display name used in help.
		 * @param description Explanation shown in help text.
		 * @param required Whether the positional is mandatory (default true).
		 */
		TypedPositional(std::string name, std::string description, bool required = true) : name_(std::move(name)), description_(std::move(description)), required_(required) {
		}

		/** @name Accessors */
		///@{
		[[nodiscard]] const std::string &name() const noexcept {
			return name_;
		}
		[[nodiscard]] const std::string &description() const noexcept {
			return description_;
		}
		[[nodiscard]] bool is_required() const noexcept {
			return required_;
		}
		[[nodiscard]] bool has_value() const noexcept {
			return value_.has_value();
		}
		[[nodiscard]] const std::optional<T> &value() const noexcept {
			return value_;
		}
		///@}

		/**
		 * @brief Parse and set the value from a string, then validate if set.
		 * @param str Raw token from the command line.
		 * @return Result<void> ok() if parsed (+validated), err(Error) otherwise.
		 */
		Result<void> set_value_from_string(std::string_view str) {
			auto converted = ValueConverter<T>::from_string(str);
			if (! converted) {
				return Result<void>::err(converted.error());
			}

			value_ = std::move(converted.value());

			if (validator_) {
				return validator_(*value_);
			}

			return Result<void>::ok();
		}

		/**
		 * @brief Provide a custom validator function.
		 * @param fn Validator invoked after parsing.
		 */
		void set_validator(Validator<T> fn) {
			validator_ = std::move(fn);
		}

	  private:
		std::string name_;
		std::string description_;
		bool required_;
		std::optional<T> value_;
		Validator<T> validator_;
	};

}// namespace cli

#endif// CPPLI_TYPES_HPP
