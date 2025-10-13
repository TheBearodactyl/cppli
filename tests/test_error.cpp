#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <cppli_error.hpp>

using namespace cli;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("Error creation and formatting", "[error]") {
	SECTION("Default error has None code") {
		Error err;
		REQUIRE(err.code() == ErrorCode::None);
		REQUIRE(err.message().empty());
	}

	SECTION("Error with code and message") {
		Error err(ErrorCode::UnknownFlag, "Test error");
		REQUIRE(err.code() == ErrorCode::UnknownFlag);
		REQUIRE(err.message() == "Test error");
	}

	SECTION("Error formatting includes message") {
		Error err(ErrorCode::InvalidFlagValue, "Invalid value");
		std::string formatted = err.format();
		REQUIRE_THAT(formatted, ContainsSubstring("Invalid value"));
	}
}

TEST_CASE("Error factory methods", "[error]") {
	SECTION("unknown_flag creates correct error") {
		auto err = Error::unknown_flag("verbose");
		REQUIRE(err.code() == ErrorCode::UnknownFlag);
		REQUIRE_THAT(err.message(), ContainsSubstring("verbose"));
	}

	SECTION("missing_required_flag creates correct error") {
		auto err = Error::missing_required_flag("output");
		REQUIRE(err.code() == ErrorCode::MissingRequiredFlag);
		REQUIRE_THAT(err.message(), ContainsSubstring("output"));
		REQUIRE_THAT(err.message(), ContainsSubstring("--"));
	}

	SECTION("missing_required_positional creates correct error") {
		auto err = Error::missing_required_positional("filename");
		REQUIRE(err.code() == ErrorCode::MissingRequiredPositional);
		REQUIRE_THAT(err.message(), ContainsSubstring("filename"));
	}

	SECTION("invalid_flag_value creates correct error") {
		auto err = Error::invalid_flag_value("port", "abc");
		REQUIRE(err.code() == ErrorCode::InvalidFlagValue);
		REQUIRE_THAT(err.message(), ContainsSubstring("port"));
		REQUIRE_THAT(err.message(), ContainsSubstring("abc"));
	}

	SECTION("too_many_positionals creates correct error") {
		auto err = Error::too_many_positionals();
		REQUIRE(err.code() == ErrorCode::TooManyPositionals);
	}

	SECTION("validation_failed creates correct error") {
		auto err = Error::validation_failed("port", "must be positive");
		REQUIRE(err.code() == ErrorCode::ValidationFailed);
		REQUIRE_THAT(err.message(), ContainsSubstring("port"));
		REQUIRE_THAT(err.message(), ContainsSubstring("must be positive"));
	}
}

TEST_CASE("Result<void> operations", "[error][result]") {
	SECTION("Success result") {
		Result<void> result = Result<void>::ok();
		REQUIRE(result.has_value());
		REQUIRE(result);
	}

	SECTION("Error result") {
		Result<void> result = Result<void>::err(Error::unknown_flag("test"));
		REQUIRE_FALSE(result.has_value());
		REQUIRE_FALSE(result);
		REQUIRE(result.error().code() == ErrorCode::UnknownFlag);
	}

	SECTION("Accessing error on success throws") {
		Result<void> result = Result<void>::ok();
		REQUIRE_THROWS(result.error());
	}
}

TEST_CASE("Result<T> operations", "[error][result]") {
	SECTION("Success result with value") {
		Result<int> result = Result<int>::ok(42);
		REQUIRE(result.has_value());
		REQUIRE(result);
		REQUIRE(result.value() == 42);
	}

	SECTION("Error result") {
		Result<int> result = Result<int>::err(Error::invalid_flag_value("count", "abc"));
		REQUIRE_FALSE(result.has_value());
		REQUIRE_FALSE(result);
		REQUIRE(result.error().code() == ErrorCode::InvalidFlagValue);
	}

	SECTION("value_or returns default on error") {
		Result<int> result = Result<int>::err(Error(ErrorCode::None, ""));
		REQUIRE(result.value_or(100) == 100);
	}

	SECTION("value_or returns value on success") {
		Result<int> result = Result<int>::ok(42);
		REQUIRE(result.value_or(100) == 42);
	}

	SECTION("Accessing value on error throws") {
		Result<int> result = Result<int>::err(Error(ErrorCode::None, ""));
		REQUIRE_THROWS(result.value());
	}

	SECTION("Accessing error on success throws") {
		Result<int> result = Result<int>::ok(42);
		REQUIRE_THROWS(result.error());
	}
}

TEST_CASE("Result copy and move semantics", "[error][result]") {
	SECTION("Result<int> can be copied") {
		Result<int> result1 = Result<int>::ok(42);
		Result<int> result2 = result1;
		REQUIRE(result2.value() == 42);
	}

	SECTION("Result<int> can be moved") {
		Result<int> result1 = Result<int>::ok(42);
		Result<int> result2 = std::move(result1);
		REQUIRE(result2.value() == 42);
	}
}
