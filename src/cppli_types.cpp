#include <charconv>
#include <cppli_error.hpp>
#include <cppli_types.hpp>
#include <string>
#include <system_error>

namespace cli {
	Result<int> ValueConverter<int>::from_string(std::string_view str) {
		int value{};
		auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);

		if (ec == std::errc()) {
			return Result<int>::ok(value);
		}

		if (ec == std::errc::invalid_argument) {
			return Result<int>::err(Error(ErrorCode::InvalidFlagValue, "Invalid integer format"));
		}

		if (ec == std::errc::result_out_of_range) {
			return Result<int>::err(Error(ErrorCode::InvalidFlagValue, "Integer out of range"));
		}

		return Result<int>::err(Error(ErrorCode::InvalidFlagValue, "Unknown conversion error"));
	}

	Result<double> ValueConverter<double>::from_string(std::string_view str) {
		double value{};
		auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);

		if (ec == std::errc()) {
			return Result<double>::ok(value);
		}

		if (ec == std::errc::invalid_argument) {
			return Result<double>::err(Error(ErrorCode::InvalidFlagValue, "Invalid floating-point format"));
		}

		if (ec == std::errc::result_out_of_range) {
			return Result<double>::err(Error(ErrorCode::InvalidFlagValue, "Floating-point out of range"));
		}

		return Result<double>::err(Error(ErrorCode::InvalidFlagValue, "Unknown conversion error"));
	}

	Result<bool> ValueConverter<bool>::from_string(std::string_view str) {
		if (str == "true" || str == "1" || str == "yes" || str == "on") {
			return Result<bool>::ok(true);
		}

		if (str == "false" || str == "0" || str == "no" || str == "off") {
			return Result<bool>::ok(false);
		}

		return Result<bool>::err(Error(ErrorCode::InvalidFlagValue, "Invalid boolean value (expected: true/false, 1/0, yes/no, on/off)"));
	}
}// namespace cli
