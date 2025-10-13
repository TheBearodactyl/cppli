#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <cppli.hpp>

using namespace cli;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("Parser basic construction", "[parser]") {
	SECTION("Create parser with name only") {
		Parser parser("myapp");
		REQUIRE_NOTHROW(parser.generate_help());
	}

	SECTION("Create parser with description") {
		Parser parser("myapp", "A test application");
		std::string help = parser.generate_help();
		REQUIRE_THAT(help, ContainsSubstring("myapp"));
		REQUIRE_THAT(help, ContainsSubstring("A test application"));
	}

	SECTION("Create parser with version") {
		Parser parser("myapp", "A test application", "1.0.0");
		std::string help = parser.generate_help();
		REQUIRE_THAT(help, ContainsSubstring("1.0.0"));
	}
}

TEST_CASE("Parser flag management", "[parser]") {
	SECTION("Add string flag") {
		Parser parser("myapp");
		parser.add_flag<std::string>("output", "Output file");
		REQUIRE_FALSE(parser.has("output"));
	}

	SECTION("Add integer flag with short name") {
		Parser parser("myapp");
		parser.add_flag<int>("port", "Port number").set_short_name("p");

		std::vector<std::string> args = {"-p", "8080"};
		auto result = parser.parse(args);
		REQUIRE(result.has_value());
		REQUIRE(parser.has("port"));
		REQUIRE(parser.get<int>("port") == 8080);
	}

	SECTION("Add flag with default value") {
		Parser parser("myapp");
		parser.add_flag<int>("threads", "Thread count").set_default_value(4);

		std::vector<std::string> args = {};
		auto result = parser.parse(args);
		REQUIRE(result.has_value());
		REQUIRE(parser.get<int>("threads") == 4);
	}

	SECTION("Required flag must be provided") {
		Parser parser("myapp");
		parser.add_flag<std::string>("config", "Config file").set_required();

		std::vector<std::string> args = {};
		auto result = parser.parse(args);
		REQUIRE_FALSE(result.has_value());
		REQUIRE(result.error().code() == ErrorCode::MissingRequiredFlag);
	}
}

TEST_CASE("Parser parsing operations", "[parser]") {
	SECTION("Parse long flag with equals") {
		Parser parser("myapp");
		parser.add_flag<std::string>("output", "Output file");

		std::vector<std::string> args = {"--output=result.txt"};
		auto result = parser.parse(args);
		REQUIRE(result.has_value());
		REQUIRE(parser.get<std::string>("output") == "result.txt");
	}

	SECTION("Parse long flag with space") {
		Parser parser("myapp");
		parser.add_flag<std::string>("output", "Output file");

		std::vector<std::string> args = {"--output", "result.txt"};
		auto result = parser.parse(args);
		REQUIRE(result.has_value());
		REQUIRE(parser.get<std::string>("output") == "result.txt");
	}

	SECTION("Parse short flag") {
		Parser parser("myapp");
		parser.add_flag<int>("count", "Count").set_short_name("c");

		std::vector<std::string> args = {"-c", "10"};
		auto result = parser.parse(args);
		REQUIRE(result.has_value());
		REQUIRE(parser.get<int>("count") == 10);
	}

	SECTION("Parse boolean flag") {
		Parser parser("myapp");
		parser.add_flag<bool>("verbose", "Verbose output").set_short_name("v");

		std::vector<std::string> args = {"-v"};
		auto result = parser.parse(args);
		REQUIRE(result.has_value());
		REQUIRE(parser.get<bool>("verbose") == true);
	}

	SECTION("Unknown flag returns error") {
		Parser parser("myapp");

		std::vector<std::string> args = {"--unknown"};
		auto result = parser.parse(args);
		REQUIRE_FALSE(result.has_value());
		REQUIRE(result.error().code() == ErrorCode::UnknownFlag);
	}

	SECTION("Invalid integer value returns error") {
		Parser parser("myapp");
		parser.add_flag<int>("port", "Port number");

		std::vector<std::string> args = {"--port", "not-a-number"};
		auto result = parser.parse(args);
		REQUIRE_FALSE(result.has_value());
	}
}

TEST_CASE("Parser positional arguments", "[parser]") {
	SECTION("Add and parse positional argument") {
		Parser parser("myapp");
		parser.add_positional<std::string>("input", "Input file");

		std::vector<std::string> args = {"data.txt"};
		auto result = parser.parse(args);
		REQUIRE(result.has_value());
		REQUIRE(parser.get_positional<std::string>(0) == "data.txt");
	}

	SECTION("Parse positional by name") {
		Parser parser("myapp");
		parser.add_positional<std::string>("input", "Input file");

		std::vector<std::string> args = {"data.txt"};
		auto result = parser.parse(args);
		REQUIRE(result.has_value());
		REQUIRE(parser.get_positional<std::string>("input") == "data.txt");
	}

	SECTION("Multiple positional arguments") {
		Parser parser("myapp");
		parser.add_positional<std::string>("input", "Input file");
		parser.add_positional<std::string>("output", "Output file");

		std::vector<std::string> args = {"in.txt", "out.txt"};
		auto result = parser.parse(args);
		REQUIRE(result.has_value());
		REQUIRE(parser.get_positional<std::string>(0) == "in.txt");
		REQUIRE(parser.get_positional<std::string>(1) == "out.txt");
	}

	SECTION("Optional positional not provided") {
		Parser parser("myapp");
		parser.add_positional<std::string>("input", "Input file", true);
		parser.add_positional<std::string>("output", "Output file", false);

		std::vector<std::string> args = {"in.txt"};
		auto result = parser.parse(args);
		REQUIRE(result.has_value());
		REQUIRE_FALSE(parser.get_positional<std::string>(1).has_value());
	}

	SECTION("Too many positionals returns error") {
		Parser parser("myapp");
		parser.add_positional<std::string>("input", "Input file");

		std::vector<std::string> args = {"file1.txt", "file2.txt"};
		auto result = parser.parse(args);
		REQUIRE_FALSE(result.has_value());
		REQUIRE(result.error().code() == ErrorCode::TooManyPositionals);
	}

	SECTION("Required positional missing returns error") {
		Parser parser("myapp");
		parser.add_positional<std::string>("input", "Input file", true);

		std::vector<std::string> args = {};
		auto result = parser.parse(args);
		REQUIRE_FALSE(result.has_value());
		REQUIRE(result.error().code() == ErrorCode::MissingRequiredPositional);
	}
}

TEST_CASE("Parser double dash separator", "[parser]") {
	SECTION("Arguments after -- are positional") {
		Parser parser("myapp");
		parser.add_flag<std::string>("flag", "A flag");
		parser.add_positional<std::string>("input", "Input file");

		std::vector<std::string> args = {"--", "--flag"};
		auto result = parser.parse(args);
		REQUIRE(result.has_value());
		REQUIRE_FALSE(parser.has("flag"));
		REQUIRE(parser.get_positional<std::string>(0) == "--flag");
	}

	SECTION("Flags before -- are parsed normally") {
		Parser parser("myapp");
		parser.add_flag<bool>("verbose", "Verbose").set_short_name("v");
		parser.add_positional<std::string>("input", "Input file");

		std::vector<std::string> args = {"-v", "--", "file.txt"};
		auto result = parser.parse(args);
		REQUIRE(result.has_value());
		REQUIRE(parser.get<bool>("verbose") == true);
		REQUIRE(parser.get_positional<std::string>(0) == "file.txt");
	}
}

TEST_CASE("Parser mixed flags and positionals", "[parser]") {
	SECTION("Flags and positionals intermixed") {
		Parser parser("myapp");
		parser.add_flag<int>("threads", "Thread count").set_short_name("t");
		parser.add_flag<bool>("verbose", "Verbose").set_short_name("v");
		parser.add_positional<std::string>("input", "Input file");
		parser.add_positional<std::string>("output", "Output file");

		std::vector<std::string> args = {"-t", "4", "in.txt", "-v", "out.txt"};
		auto result = parser.parse(args);
		REQUIRE(result.has_value());
		REQUIRE(parser.get<int>("threads") == 4);
		REQUIRE(parser.get<bool>("verbose") == true);
		REQUIRE(parser.get_positional<std::string>(0) == "in.txt");
		REQUIRE(parser.get_positional<std::string>(1) == "out.txt");
	}
}

TEST_CASE("Parser flag validation", "[parser]") {
	SECTION("Choices validation succeeds") {
		Parser parser("myapp");
		parser.add_flag<std::string>("format", "Output format").set_choices({"json", "xml", "yaml"});

		std::vector<std::string> args = {"--format", "json"};
		auto result = parser.parse(args);
		REQUIRE(result.has_value());
	}

	SECTION("Choices validation fails") {
		Parser parser("myapp");
		parser.add_flag<std::string>("format", "Output format").set_choices({"json", "xml", "yaml"});

		std::vector<std::string> args = {"--format", "html"};
		auto result = parser.parse(args);
		REQUIRE_FALSE(result.has_value());
		REQUIRE(result.error().code() == ErrorCode::ValidationFailed);
	}

	SECTION("Custom validation succeeds") {
		Parser parser("myapp");
		parser.add_flag<int>("port", "Port number").set_validator([](const int &val) -> Result<void> {
			if (val >= 1024 && val <= 65535) {
				return Result<void>::ok();
			}
			return Result<void>::err(Error::validation_failed("port", "must be between 1024 and 65535"));
		});

		std::vector<std::string> args = {"--port", "8080"};
		auto result = parser.parse(args);
		REQUIRE(result.has_value());
	}

	SECTION("Custom validation fails") {
		Parser parser("myapp");
		parser.add_flag<int>("port", "Port number").set_validator([](const int &val) -> Result<void> {
			if (val >= 1024 && val <= 65535) {
				return Result<void>::ok();
			}
			return Result<void>::err(Error::validation_failed("port", "must be between 1024 and 65535"));
		});

		std::vector<std::string> args = {"--port", "80"};
		auto result = parser.parse(args);
		REQUIRE_FALSE(result.has_value());
	}
}

TEST_CASE("Parser type safety", "[parser]") {
	SECTION("Get with correct type") {
		Parser parser("myapp");
		parser.add_flag<int>("count", "Count");

		std::vector<std::string> args = {"--count", "42"};
		auto _ = parser.parse(args);

		auto value = parser.get<int>("count");
		REQUIRE(value.has_value());
		REQUIRE(value.value() == 42);
	}

	SECTION("Get with wrong type returns nullopt") {
		Parser parser("myapp");
		parser.add_flag<int>("count", "Count");

		std::vector<std::string> args = {"--count", "42"};
		auto _ = parser.parse(args);

		auto value = parser.get<std::string>("count");
		REQUIRE_FALSE(value.has_value());
	}

	SECTION("Get non-existent flag returns nullopt") {
		Parser parser("myapp");
		auto value = parser.get<std::string>("nonexistent");
		REQUIRE_FALSE(value.has_value());
	}
}

TEST_CASE("Parser examples", "[parser]") {
	SECTION("Add and generate examples") {
		Parser parser("myapp");
		parser.add_example("Process a file", "myapp --input data.txt");
		parser.add_example("Verbose mode", "myapp -v --config app.conf");

		std::string help = parser.generate_help();
		REQUIRE_THAT(help, ContainsSubstring("EXAMPLES"));
		REQUIRE_THAT(help, ContainsSubstring("Process a file"));
		REQUIRE_THAT(help, ContainsSubstring("myapp --input data.txt"));
	}
}

TEST_CASE("Parser help generation", "[parser]") {
	SECTION("Generate help with flags") {
		Parser parser("myapp", "Test application", "1.0.0");
		parser.add_flag<std::string>("output", "Output file").set_short_name("o");
		parser.add_flag<int>("threads", "Thread count").set_default_value(4);

		std::string help = parser.generate_help();
		REQUIRE_THAT(help, ContainsSubstring("myapp"));
		REQUIRE_THAT(help, ContainsSubstring("1.0.0"));
		REQUIRE_THAT(help, ContainsSubstring("Test application"));
		REQUIRE_THAT(help, ContainsSubstring("USAGE"));
		REQUIRE_THAT(help, ContainsSubstring("OPTIONS"));
		REQUIRE_THAT(help, ContainsSubstring("--output"));
		REQUIRE_THAT(help, ContainsSubstring("-o"));
		REQUIRE_THAT(help, ContainsSubstring("--threads"));
	}

	SECTION("Generate help with positionals") {
		Parser parser("myapp");
		parser.add_positional<std::string>("input", "Input file", true);
		parser.add_positional<std::string>("output", "Output file", false);

		std::string help = parser.generate_help();
		REQUIRE_THAT(help, ContainsSubstring("<input>"));
		REQUIRE_THAT(help, ContainsSubstring("[output]"));
	}
}

TEST_CASE("Parser complex scenarios", "[parser]") {
	SECTION("Real-world command line parser") {
		Parser parser("converter", "File format converter", "2.1.0");

		parser.add_flag<std::string>("format", "Output format").set_short_name("f").set_required().set_choices({"json", "xml", "yaml"});

		parser.add_flag<int>("threads", "Number of threads").set_short_name("t").set_default_value(4).set_validator([](const int &val) -> Result<void> {
			if (val >= 1 && val <= 32) {
				return Result<void>::ok();
			}
			return Result<void>::err(Error::validation_failed("threads", "must be between 1 and 32"));
		});

		parser.add_flag<bool>("verbose", "Enable verbose output").set_short_name("v");

		parser.add_positional<std::string>("input", "Input file");
		parser.add_positional<std::string>("output", "Output file", false);

		parser.add_example("Convert to JSON", "converter -f json input.txt output.json");
		parser.add_example("With custom threads", "converter -f xml -t 8 data.txt");

		std::vector<std::string> args = {"-f", "json", "-t", "8", "-v", "input.csv", "output.json"};

		auto result = parser.parse(args);
		REQUIRE(result.has_value());
		REQUIRE(parser.get<std::string>("format") == "json");
		REQUIRE(parser.get<int>("threads") == 8);
		REQUIRE(parser.get<bool>("verbose") == true);
		REQUIRE(parser.get_positional<std::string>("input") == "input.csv");
		REQUIRE(parser.get_positional<std::string>("output") == "output.json");
	}

	SECTION("Comprehensive error handling") {
		Parser parser("myapp");
		parser.add_flag<int>("port", "Port number").set_required();
		parser.add_positional<std::string>("config", "Config file");

		std::vector<std::string> args1 = {"config.yml"};
		auto result1 = parser.parse(args1);
		REQUIRE_FALSE(result1.has_value());
		REQUIRE(result1.error().code() == ErrorCode::MissingRequiredFlag);
		REQUIRE_THAT(result1.error().message(), ContainsSubstring("port"));

		Parser parser2("myapp");
		parser2.add_flag<int>("port", "Port number");
		std::vector<std::string> args2 = {"--port", "abc"};
		auto result2 = parser2.parse(args2);
		REQUIRE_FALSE(result2.has_value());

		Parser parser3("myapp");
		std::vector<std::string> args3 = {"--unknown-flag"};
		auto result3 = parser3.parse(args3);
		REQUIRE_FALSE(result3.has_value());
		REQUIRE(result3.error().code() == ErrorCode::UnknownFlag);
	}
}

TEST_CASE("Parser edge cases", "[parser]") {
	SECTION("Empty arguments") {
		Parser parser("myapp");
		parser.add_flag<bool>("help", "Help").set_short_name("h");

		std::vector<std::string> args = {};
		auto result = parser.parse(args);
		REQUIRE(result.has_value());
	}

	SECTION("Flag value starts with dash") {
		Parser parser("myapp");
		parser.add_positional<std::string>("value", "A value");

		std::vector<std::string> args = {"--", "-weird-value"};
		auto result = parser.parse(args);
		REQUIRE(result.has_value());
		REQUIRE(parser.get_positional<std::string>(0) == "-weird-value");
	}

	SECTION("Multiple flags of different types") {
		Parser parser("myapp");
		parser.add_flag<std::string>("name", "Name");
		parser.add_flag<int>("age", "Age");
		parser.add_flag<double>("score", "Score");
		parser.add_flag<bool>("active", "Active");

		std::vector<std::string> args = {"--name", "Alice", "--age", "30", "--score", "95.5", "--active", "true"};

		auto result = parser.parse(args);
		REQUIRE(result.has_value());
		REQUIRE(parser.get<std::string>("name") == "Alice");
		REQUIRE(parser.get<int>("age") == 30);
		REQUIRE(parser.get<double>("score") == 95.5);
		REQUIRE(parser.get<bool>("active") == true);
	}
}
