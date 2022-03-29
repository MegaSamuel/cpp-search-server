#include "log_duration.h"

LogDuration::LogDuration(const std::string& str, std::ostream& output) : text_(str), output_(output) {
}

LogDuration::LogDuration(const std::string_view& str, std::ostream& output) : text_(str), output_(output) {
}

LogDuration::~LogDuration() {
    using namespace std::chrono;
    using namespace std::literals;

    const auto end_time = Clock::now();
    const auto dur = end_time - start_time_;
    output_ << text_ << ": "s << duration_cast<milliseconds>(dur).count() << " ms"s << std::endl;
}
