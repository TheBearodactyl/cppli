#ifndef CPPLI_ERROR_HPP
#define CPPLI_ERROR_HPP

#include <algorithm>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

namespace cli {

	/**
	 * @brief Error categories produced by parsing and validation.
	 */
	enum class ErrorCode {
		None,					  ///< No error
		UnknownFlag,			  ///< Flag not recognized
		MissingRequiredFlag,	  ///< A required flag was not provided
		MissingRequiredPositional,///< A required positional was not provided
		InvalidFlagValue,		  ///< Failed to parse or invalid value for a flag
		TooManyPositionals,		  ///< More positional args than declared
		MissingFlagValue,		  ///< Flag expected a value but none was given
		ValidationFailed,		  ///< User-provided validator rejected the value
		ParserNotInitialized,	  ///< Reserved for future use
	};

	/**
	 * @brief Structured error with code, message, and source location.
	 *
	 * Error carries both a machine-readable code and a human-readable message.
	 * When compiled without NDEBUG, format() also includes file and line.
	 */
	class Error {
	  public:
		/**
		 * @brief Default constructor for "no error".
		 */
		Error() = default;

		/**
		 * @brief Construct an Error with code and message.
		 * @param code The error category.
		 * @param message Descriptive message.
		 * @param location Source location (defaults to callsite).
		 */
		Error(ErrorCode code, std::string message, std::source_location location = std::source_location::current()) : code_(code), message_(std::move(message)), location_(location) {
		}

		/**
		 * @brief Get the error code.
		 */
		[[nodiscard]] ErrorCode code() const noexcept {
			return code_;
		}

		/**
		 * @brief Get the human-readable message.
		 */
		[[nodiscard]] const std::string &message() const noexcept {
			return message_;
		}

		/**
		 * @brief Get the capture of source location information.
		 */
		[[nodiscard]] const std::source_location &location() const noexcept {
			return location_;
		}

		/**
		 * @brief Produce a formatted message for display.
		 *
		 * In debug builds (without NDEBUG), includes file and line where the
		 * Error was constructed.
		 */
		[[nodiscard]] std::string format() const;

		// Factory helpers to create specific error flavors:

		/**
		 * @brief Unknown flag name (e.g. "--bogus").
		 */
		[[nodiscard]] static Error unknown_flag(std::string_view flag_name);

		/**
		 * @brief Required flag was not provided.
		 */
		[[nodiscard]] static Error missing_required_flag(std::string_view flag_name);

		/**
		 * @brief Required positional was not provided.
		 */
		[[nodiscard]] static Error missing_required_positional(std::string_view pos_name);

		/**
		 * @brief Invalid value for a known flag.
		 */
		[[nodiscard]] static Error invalid_flag_value(std::string_view flag_name, std::string_view value);

		/**
		 * @brief Too many positional arguments were passed.
		 */
		[[nodiscard]] static Error too_many_positionals();

		/**
		 * @brief A flag expecting a value did not receive one.
		 */
		[[nodiscard]] static Error missing_flag_value(std::string_view flag_name);

		/**
		 * @brief User validator rejected a value with a reason.
		 */
		[[nodiscard]] static Error validation_failed(std::string_view name, std::string_view reason);

	  private:
		ErrorCode code_ = ErrorCode::None;
		std::string message_;
		std::source_location location_;
	};

	/**
	 * @brief Result<T> models either a value T or an Error.
	 *
	 * - For T != void, holds either T or Error.
	 * - For T == void, specialization holds either "ok" or Error.
	 *
	 * Usage:
	 * @code
	 * cli::Result<int> r = parse_int("123");
	 * if (!r) { std::cerr << r.error().format(); }
	 * int value = r.value(); // only when r is ok
	 * @endcode
	 *
	 * Notes:
	 * - Accessing value() when in error state throws std::logic_error.
	 * - Accessing error() when ok throws std::logic_error.
	 */
	template <typename T = void>
	class Result {
	  public:
		/**
		 * @brief Construct an ok() result for void type.
		 */
		Result()
			requires std::is_same_v<T, void>
			: has_value_(true) {
		}

		/**
		 * @brief Construct an ok() result containing a value.
		 */
		Result(T value)
			requires(! std::is_same_v<T, void>)
			: storage_(std::move(value)), has_value_(true) {
		}

		/**
		 * @brief Construct an error result.
		 */
		Result(Error error) : storage_(std::move(error)), has_value_(false) {
		}

		/**
		 * @brief True if result is ok.
		 */
		[[nodiscard]] bool has_value() const noexcept {
			return has_value_;
		}

		/**
		 * @brief Contextual boolean: true when ok.
		 */
		[[nodiscard]] explicit operator bool() const noexcept {
			return has_value_;
		}

		/**
		 * @brief Access the error. Throws if ok.
		 */
		[[nodiscard]] const Error &error() const {
			if (has_value_) {
				throw std::logic_error("Attempted to access error on successful Result");
			}
			return std::get<Error>(storage_);
		}

		/**
		 * @brief Mutable access to the contained value. Throws if error.
		 */
		[[nodiscard]] T &value()
			requires(! std::is_same_v<T, void>)
		{
			if (! has_value_) {
				throw std::logic_error("Attempted to access value on failed Result");
			}
			return std::get<T>(storage_);
		}

		/**
		 * @brief Const access to the contained value. Throws if error.
		 */
		[[nodiscard]] const T &value() const
			requires(! std::is_same_v<T, void>)
		{
			if (! has_value_) {
				throw std::logic_error("Attempted to access value on failed Result");
			}
			return std::get<T>(storage_);
		}

		/**
		 * @brief Get value or a provided default when error.
		 */
		[[nodiscard]] T value_or(T default_value) const
			requires(! std::is_same_v<T, void>)
		{
			return has_value_ ? std::get<T>(storage_) : std::move(default_value);
		}

		/**
		 * @brief Construct an ok() result (void).
		 */
		static Result ok()
			requires std::is_same_v<T, void>
		{
			return Result();
		}

		/**
		 * @brief Construct an ok() result with a value.
		 */
		static Result ok(T value)
			requires(! std::is_same_v<T, void>)
		{
			return Result(std::move(value));
		}

		/**
		 * @brief Construct an error result.
		 */
		static Result err(Error error) {
			return Result(std::move(error));
		}

	  private:
		std::conditional_t<std::is_same_v<T, void>, std::variant<std::monostate, Error>, std::variant<T, Error>> storage_;
		bool has_value_;
	};

	/**
	 * @brief Specialization of Result for void.
	 *
	 * Holds either ok or Error.
	 */
	template <>
	class Result<void> {
	  public:
		/**
		 * @brief Construct an ok() result.
		 */
		Result() : storage_(std::monostate{}), has_value_(true) {
		}

		/**
		 * @brief Construct an error result.
		 */
		Result(Error error) : storage_(std::move(error)), has_value_(false) {
		}

		/**
		 * @brief True if ok.
		 */
		[[nodiscard]] bool has_value() const noexcept {
			return has_value_;
		}

		/**
		 * @brief Contextual boolean: true when ok.
		 */
		[[nodiscard]] explicit operator bool() const noexcept {
			return has_value_;
		}

		/**
		 * @brief Access the error. Throws if ok.
		 */
		[[nodiscard]] const Error &error() const {
			if (has_value_) {
				throw std::logic_error("Attempted to access error on successful Result");
			}
			return std::get<Error>(storage_);
		}

		/**
		 * @brief Construct an ok() result.
		 */
		static Result ok() {
			return Result();
		}

		/**
		 * @brief Construct an error result.
		 */
		static Result err(Error error) {
			return Result(std::move(error));
		}

	  private:
		std::variant<std::monostate, Error> storage_;
		bool has_value_;
	};

}// namespace cli

#endif// CPPLI_ERROR_HPP
