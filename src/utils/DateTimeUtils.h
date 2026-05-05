#pragma once

#include <string>
#include <optional>
#include <chrono>
#include <cstdint>

namespace lecmd {

class DateTimeUtils {
public:
    using Timestamp = std::chrono::system_clock::time_point;
    using OptionalTimestamp = std::optional<Timestamp>;

    static Timestamp FromUnixSeconds(int64_t seconds);
    static Timestamp FromUnixMilliseconds(int64_t milliseconds);
    static Timestamp FromFileTime(uint64_t filetime);
    static OptionalTimestamp FromFileTimeSafe(uint64_t filetime);
    static OptionalTimestamp ParseDateTime(const std::string& dateStr);
    static std::string Format(const Timestamp& ts, const std::string& format = "%Y-%m-%d %H:%M:%S");
    static std::string Format(const OptionalTimestamp& ts, const std::string& format = "%Y-%m-%d %H:%M:%S");
    static std::string Format(std::time_t ts, const std::string& format = "%Y-%m-%d %H:%M:%S");
    static std::string FormatMicroseconds(const Timestamp& ts);
    static std::string FormatMicroseconds(std::time_t ts);
    static Timestamp Now();
    static bool IsValid(const Timestamp& ts);
    static bool IsValid(const OptionalTimestamp& ts);

private:
    static constexpr uint64_t FILETIME_UNIX_DIFF = 116444736000000000ULL;
    static constexpr uint64_t MIN_VALID_FILETIME = FILETIME_UNIX_DIFF;
};

} // namespace lecmd
