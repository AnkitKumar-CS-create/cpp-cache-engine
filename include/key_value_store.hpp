#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

class KeyValueStore {
public:
    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    bool exists(const std::string& key);
    bool incr(const std::string& key, long long& result);
    bool expire(const std::string& key, std::chrono::seconds seconds);
    long long ttl(const std::string& key);

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    struct StoredValue {
        std::string value;
        std::optional<TimePoint> expires_at;
    };

    std::unordered_map<std::string, StoredValue> data_;
    mutable std::mutex mutex_;

    void remove_if_expired(const std::string& key);
    bool parse_integer(const std::string& text, long long& value) const;
};
