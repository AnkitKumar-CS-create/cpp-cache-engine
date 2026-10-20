#pragma once

#include <chrono>
#include <cstddef>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

class KeyValueStore {
public:
    explicit KeyValueStore(std::size_t max_capacity = 10000);

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
        std::list<std::string>::iterator lru_position;
    };

    using StoreIterator = std::unordered_map<std::string, StoredValue>::iterator;

    std::unordered_map<std::string, StoredValue> data_;
    std::list<std::string> lru_keys_;
    const std::size_t max_capacity_;
    mutable std::mutex mutex_;

    void remove_if_expired(const std::string& key);
    void touch_key(StoreIterator it);
    void evict_if_needed();
    bool parse_integer(const std::string& text, long long& value) const;
};
