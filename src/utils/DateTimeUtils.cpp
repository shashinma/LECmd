#include "DateTimeUtils.h"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <regex>

namespace lecmd {

DateTimeUtils::Timestamp DateTimeUtils::FromUnixSeconds(int64_t seconds) {
    return std::chrono::system_clock::from_time_t(static_cast<time_t>(seconds));
}

DateTimeUtils::Timestamp DateTimeUtils::FromUnixMilliseconds(int64_t milliseconds) {
    auto duration = std::chrono::milliseconds(milliseconds);
    return Timestamp(std::chrono::duration_cast<std::chrono::system_clock::duration>(duration));
}

DateTimeUtils::Timestamp DateTimeUtils::FromFileTime(uint64_t filetime) {
    if (filetime < FILETIME_UNIX_DIFF) {
        return Timestamp{};
    }
    uint64_t unix_100ns = filetime - FILETIME_UNIX_DIFF;
    auto duration = std::chrono::duration<uint64_t, std::ratio<1, 10000000>>(unix_100ns);
    return Timestamp(std::chrono::duration_cast<std::chrono::system_clock::duration>(duration));
}

DateTimeUtils::OptionalTimestamp DateTimeUtils::FromFileTimeSafe(uint64_t filetime) {
    if (filetime < MIN_VALID_FILETIME || filetime == 0) {
        return std::nullopt;
    }
    auto ts = FromFileTime(filetime);
    if (!IsValid(ts)) {
        return std::nullopt;
    }
    return ts;
}

DateTimeUtils::OptionalTimestamp DateTimeUtils::ParseDateTime(const std::string& dateStr) {
    if (dateStr.empty()) {
        return std::nullopt;
    }

    std::tm tm = {};
    std::istringstream ss(dateStr);

    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (!ss.fail()) {
        return FromUnixSeconds(std::mktime(&tm));
    }

    ss.clear();
    ss.str(dateStr);
    ss >> std::get_time(&tm, "%Y-%m-%d");
    if (!ss.fail()) {
        return FromUnixSeconds(std::mktime(&tm));
    }

    ss.clear();
    ss.str(dateStr);
    ss >> std::get_time(&tm, "%m/%d/%Y");
    if (!ss.fail()) {
        return FromUnixSeconds(std::mktime(&tm));
    }

    ss.clear();
    ss.str(dateStr);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (!ss.fail()) {
        return FromUnixSeconds(std::mktime(&tm));
    }

    ss.clear();
    ss.str(dateStr);
    ss >> std::get_time(&tm, "%m/%d/%Y %H:%M:%S");
    if (!ss.fail()) {
        return FromUnixSeconds(std::mktime(&tm));
    }

    return std::nullopt;
}

std::string DateTimeUtils::Format(const Timestamp& ts, const std::string& format) {
    if (!IsValid(ts)) {
        return "";
    }

    auto time_t_val = std::chrono::system_clock::to_time_t(ts);
    std::tm tm = {};

#ifdef _WIN32
    gmtime_s(&tm, &time_t_val);
#else
    gmtime_r(&time_t_val, &tm);
#endif

    std::ostringstream ss;
    ss << std::put_time(&tm, format.c_str());
    return ss.str();
}

std::string DateTimeUtils::Format(const OptionalTimestamp& ts, const std::string& format) {
    if (!ts.has_value()) {
        return "";
    }
    return Format(*ts, format);
}

std::string DateTimeUtils::Format(std::time_t ts, const std::string& format) {
    if (ts == 0) return "";
    return Format(FromUnixSeconds(ts), format);
}

std::string DateTimeUtils::FormatMicroseconds(const Timestamp& ts) {
    if (!IsValid(ts)) {
        return "";
    }

    auto time_t_val = std::chrono::system_clock::to_time_t(ts);
    std::tm tm = {};

#ifdef _WIN32
    gmtime_s(&tm, &time_t_val);
#else
    gmtime_r(&time_t_val, &tm);
#endif

    auto since_epoch = ts.time_since_epoch();
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(since_epoch);
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(since_epoch - secs);

    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(6) << micros.count();
    return ss.str();
}

DateTimeUtils::Timestamp DateTimeUtils::Now() {
    return std::chrono::system_clock::now();
}

bool DateTimeUtils::IsValid(const Timestamp& ts) {
    static const Timestamp epoch{};
    if (ts == epoch) {
        return false;
    }
    auto time_t_val = std::chrono::system_clock::to_time_t(ts);
    if (time_t_val <= 0) {
        return false;
    }
    constexpr time_t max_reasonable = 4102444800;
    if (time_t_val > max_reasonable) {
        return false;
    }
    return true;
}

bool DateTimeUtils::IsValid(const OptionalTimestamp& ts) {
    if (!ts.has_value()) {
        return false;
    }
    return IsValid(*ts);
}

} // namespace lecmd
