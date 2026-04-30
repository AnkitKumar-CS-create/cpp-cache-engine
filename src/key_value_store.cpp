#include "key_value_store.hpp"

#include <cstdlib>
#include <limits>
#include <mutex>

void KeyValueStore::set(const std::string& key, const std::string& value) {
    std::scoped_lock lock(mutex_);
    data_[key] = StoredValue{value, std::nullopt};
}

std::optional<std::string> KeyValueStore::get(const std::string& key) {
    std::scoped_lock lock(mutex_);
    remove_if_expired(key);

    auto it = data_.find(key);
    if (it == data_.end()) {
        return std::nullopt;
    }

    return it->second.value;
}

bool KeyValueStore::del(const std::string& key) {
    std::scoped_lock lock(mutex_);
    remove_if_expired(key);
    return data_.erase(key) > 0;
}

bool KeyValueStore::exists(const std::string& key) {
    std::scoped_lock lock(mutex_);
    remove_if_expired(key);
    return data_.find(key) != data_.end();
}

bool KeyValueStore::incr(const std::string& key, long long& result) {
    std::scoped_lock lock(mutex_);
    remove_if_expired(key);

    auto it = data_.find(key);
    if (it == data_.end()) {
        result = 1;
        data_[key] = StoredValue{"1", std::nullopt};
        return true;
    }

    long long current = 0;
    if (!parse_integer(it->second.value, current)) {
        return false;
    }

    if (current == std::numeric_limits<long long>::max()) {
        return false;
    }

    result = current + 1;
    it->second.value = std::to_string(result);
    return true;
}

bool KeyValueStore::expire(const std::string& key, std::chrono::seconds seconds) {
    std::scoped_lock lock(mutex_);
    remove_if_expired(key);

    auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }

    it->second.expires_at = Clock::now() + seconds;
    return true;
}

long long KeyValueStore::ttl(const std::string& key) {
    std::scoped_lock lock(mutex_);
    remove_if_expired(key);

    auto it = data_.find(key);
    if (it == data_.end()) {
        return -2;
    }

    if (!it->second.expires_at.has_value()) {
        return -1;
    }

    auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
        *it->second.expires_at - Clock::now());

    if (remaining.count() < 0) {
        data_.erase(it);
        return -2;
    }

    return remaining.count();
}

void KeyValueStore::remove_if_expired(const std::string& key) {
    auto it = data_.find(key);
    if (it == data_.end() || !it->second.expires_at.has_value()) {
        return;
    }

    if (Clock::now() >= *it->second.expires_at) {
        data_.erase(it);
    }
}

bool KeyValueStore::parse_integer(const std::string& text, long long& value) const {
    if (text.empty()) {
        return false;
    }

    std::size_t parsed_chars = 0;

    try {
        value = std::stoll(text, &parsed_chars);
    } catch (...) {
        return false;
    }

    return parsed_chars == text.size();
}
