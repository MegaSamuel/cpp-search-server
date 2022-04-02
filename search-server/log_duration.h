#pragma once

#include <chrono>
#include <iostream>
#include <string>
#include <string_view>


#define PROFILE_CONCAT_INTERNAL(X, Y) X ## Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION(x) LogDuration UNIQUE_VAR_NAME_PROFILE(x)

#define LOG_DURATION_STREAM(x, out) LogDuration UNIQUE_VAR_NAME_PROFILE(x, out);

class LogDuration {
public:
    // заменим имя типа std::chrono::steady_clock
    // с помощью using для удобства
    using Clock = std::chrono::steady_clock;

    LogDuration(const std::string& str, std::ostream& output = std::cerr);

    LogDuration(const std::string_view& str, std::ostream& output = std::cerr);

    ~LogDuration();

private:
    const Clock::time_point start_time_ = Clock::now();

    std::string text_;
    std::ostream& output_;
};
