#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <cppli_types.hpp>

using namespace cli;

TEST_CASE("ValueConverter<std::string>", "[types][converter]") {
	SECTION("Converts string successfully") {
		auto result = ValueConverter<std::string>::from_string("hello");
		REQUIRE(result.has_value());
		REQUIRE(result.value() == "hello");
	}

	SECTION("Handles empty string") {
		auto result = ValueConverter<std::string>::from_string("");
		REQUIRE(result.has_value());
		REQUIRE(result.value() == "");
	}
}

TEST_CASE("ValueConverter<int>", "[types][converter]") {
	SECTION("Converts valid integer") {
		auto result = ValueConverter<int>::from_string("42");
		REQUIRE(result.has_value());
		REQUIRE(result.value() == 42);
	}

	SECTION("Converts negative integer") {
		auto result = ValueConverter<int>::from_string("-42");
		REQUIRE(result.has_value());
		REQUIRE(result.value() == -42);
	}

	SECTION("Fails on invalid format") {
		auto result = ValueConverter<int>::from_string("abc");
		REQUIRE_FALSE(result.has_value());
		REQUIRE(result.error().code() == ErrorCode::InvalidFlagValue);
	}

	SECTION("Fails on out of range") {
		auto result = ValueConverter<int>::from_string("999999999999999999999");
		REQUIRE_FALSE(result.has_value());
	}
}

TEST_CASE("ValueConverter<double>", "[types][converter]") {
	SECTION("Converts valid double") {
		auto result = ValueConverter<double>::from_string("3.14");
		REQUIRE(result.has_value());
		REQUIRE(result.value() == 3.14);
	}

	SECTION("Converts scientific notation") {
		auto result = ValueConverter<double>::from_string("1.5e2");
		REQUIRE(result.has_value());
		REQUIRE(result.value() == 150.0);
	}

	SECTION("Fails on invalid format") {
		auto result = ValueConverter<double>::from_string("not-a-number");
		REQUIRE_FALSE(result.has_value());
	}
}

TEST_CASE("ValueConverter<bool>", "[types][converter]") {
	SECTION("Converts 'true'") {
		auto result = ValueConverter<bool>::from_string("true");
		REQUIRE(result.has_value());
		REQUIRE(result.value() == true);
	}

	SECTION("Converts '1'") {
		auto result = ValueConverter<bool>::from_string("1");
		REQUIRE(result.has_value());
		REQUIRE(result.value() == true);
	}

	SECTION("Converts 'yes'") {
		auto result = ValueConverter<bool>::from_string("yes");
		REQUIRE(result.has_value());
		REQUIRE(result.value() == true);
	}

	SECTION("Converts 'false'") {
		auto result = ValueConverter<bool>::from_string("false");
		REQUIRE(result.has_value());
		REQUIRE(result.value() == false);
	}

	SECTION("Converts '0'") {
		auto result = ValueConverter<bool>::from_string("0");
		REQUIRE(result.has_value());
		REQUIRE(result.value() == false);
	}

	SECTION("Fails on invalid value") {
		auto result = ValueConverter<bool>::from_string("maybe");
		REQUIRE_FALSE(result.has_value());
	}
}

TEST_CASE("TypedFlag basic operations", "[types][flag]") {
	SECTION("Create flag with name and description") {
		TypedFlag<std::string> flag("output", "Output file");
		REQUIRE(flag.long_name() == "output");
		REQUIRE(flag.description() == "Output file");
		REQUIRE_FALSE(flag.has_value());
		REQUIRE_FALSE(flag.is_required());
	}

	SECTION("Set short name") {
		TypedFlag<std::string> flag("verbose", "Enable verbose output");
		flag.set_short_name("v");
		REQUIRE(flag.short_name() == "v");
	}

	SECTION("Set required") {
		TypedFlag<std::string> flag("config", "Config file");
		flag.set_required();
		REQUIRE(flag.is_required());
	}

	SECTION("Set default value") {
		TypedFlag<int> flag("port", "Port number");
		flag.set_default_value(8080);
		REQUIRE(flag.has_value());
		REQUIRE(flag.value() == 8080);
	}
}

TEST_CASE("TypedFlag value setting", "[types][flag]") {
	SECTION("Set string value") {
		TypedFlag<std::string> flag("name", "Your name");
		auto result = flag.set_value_from_string("Alice");
		REQUIRE(result.has_value());
		REQUIRE(flag.has_value());
		REQUIRE(flag.value() == "Alice");
	}

	SECTION("Set integer value") {
		TypedFlag<int> flag("count", "Item count");
		auto result = flag.set_value_from_string("42");
		REQUIRE(result.has_value());
		REQUIRE(flag.value() == 42);
	}

	SECTION("Fail on invalid integer") {
		TypedFlag<int> flag("count", "Item count");
		auto result = flag.set_value_from_string("not-a-number");
		REQUIRE_FALSE(result.has_value());
	}

	SECTION("Set double value") {
		TypedFlag<double> flag("ratio", "Ratio value");
		auto result = flag.set_value_from_string("0.5");
		REQUIRE(result.has_value());
		REQUIRE(flag.value() == 0.5);
	}

	SECTION("Set boolean value") {
		TypedFlag<bool> flag("verbose", "Verbose mode");
		auto result = flag.set_value_from_string("true");
		REQUIRE(result.has_value());
		REQUIRE(flag.value() == true);
	}
}

TEST_CASE("TypedFlag validation with choices", "[types][flag]") {
	SECTION("Valid choice passes validation") {
		TypedFlag<std::string> flag("format", "Output format");
		flag.set_choices({"json", "xml", "yaml"});

		auto result = flag.set_value_from_string("json");
		REQUIRE(result.has_value());
	}

	SECTION("Invalid choice fails validation") {
		TypedFlag<std::string> flag("format", "Output format");
		flag.set_choices({"json", "xml", "yaml"});

		auto result = flag.set_value_from_string("html");
		REQUIRE_FALSE(result.has_value());
		REQUIRE(result.error().code() == ErrorCode::ValidationFailed);
	}
}

TEST_CASE("TypedFlag custom validation", "[types][flag]") {
	SECTION("Custom validator accepts valid value") {
		TypedFlag<int> flag("port", "Port number");
		flag.set_validator([](const int &val) -> Result<void> {
			if (val >= 1024 && val <= 65535) {
				return Result<void>::ok();
			}
			return Result<void>::err(Error::validation_failed("port", "must be between 1024 and 65535"));
		});

		auto result = flag.set_value_from_string("8080");
		REQUIRE(result.has_value());
	}

	SECTION("Custom validator rejects invalid value") {
		TypedFlag<int> flag("port", "Port number");
		flag.set_validator([](const int &val) -> Result<void> {
			if (val >= 1024 && val <= 65535) {
				return Result<void>::ok();
			}
			return Result<void>::err(Error::validation_failed("port", "must be between 1024 and 65535"));
		});

		auto result = flag.set_value_from_string("80");
		REQUIRE_FALSE(result.has_value());
	}
}

TEST_CASE("TypedPositional operations", "[types][positional]") {
	SECTION("Create positional argument") {
		TypedPositional<std::string> pos("filename", "Input file", true);
		REQUIRE(pos.name() == "filename");
		REQUIRE(pos.description() == "Input file");
		REQUIRE(pos.is_required());
		REQUIRE_FALSE(pos.has_value());
	}

	SECTION("Set value on positional") {
		TypedPositional<std::string> pos("filename", "Input file");
		auto result = pos.set_value_from_string("data.txt");
		REQUIRE(result.has_value());
		REQUIRE(pos.has_value());
		REQUIRE(pos.value() == "data.txt");
	}

	SECTION("Set integer positional") {
		TypedPositional<int> pos("count", "Number of items");
		auto result = pos.set_value_from_string("10");
		REQUIRE(result.has_value());
		REQUIRE(pos.value() == 10);
	}

	SECTION("Optional positional") {
		TypedPositional<std::string> pos("optional", "Optional arg", false);
		REQUIRE_FALSE(pos.is_required());
	}
}
