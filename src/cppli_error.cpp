#include <cppli_error.hpp>
#include <sstream>

namespace cli {

	std::string Error::format() const {
		std::ostringstream oss;
		oss << message_;

#ifndef NDEBUG
		oss << " [" << location_.file_name() << ":" << location_.line() << "]";
#endif

		return oss.str();
	}

	Error Error::unknown_flag(std::string_view flag_name) {
		return Error(ErrorCode::UnknownFlag, std::string("Unknown flag: ") + std::string(flag_name));
	}

	Error Error::missing_required_flag(std::string_view flag_name) {
		return Error(ErrorCode::MissingRequiredFlag, std::string("Required flag missing: --") + std::string(flag_name));
	}

	Error Error::missing_required_positional(std::string_view pos_name) {
		return Error(ErrorCode::MissingRequiredPositional, std::string("Required positional missing: ") + std::string(pos_name));
	}

	Error Error::invalid_flag_value(std::string_view flag_name, std::string_view value) {
		return Error(ErrorCode::InvalidFlagValue, std::string("Invalid value for --") + std::string(flag_name) + ": " + std::string(value));
	}

	Error Error::too_many_positionals() {
		return Error(ErrorCode::TooManyPositionals, "Too many positional arguments");
	}

	Error Error::missing_flag_value(std::string_view flag_name) {
		return Error(ErrorCode::MissingFlagValue, std::string("Missing value for flag: --") + std::string(flag_name));
	}

	Error Error::validation_failed(std::string_view name, std::string_view reason) {
		return Error(ErrorCode::ValidationFailed, std::string("Validation failed for ") + std::string(name) + ": " + std::string(reason));
	}

}// namespace cli
