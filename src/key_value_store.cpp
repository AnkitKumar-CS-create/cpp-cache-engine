#include "key_value_store.hpp"

#include <iterator>
#include <limits>
#include <stdexcept>

KeyValueStore::KeyValueStore(std::size_t max_capacity)
    : max_capacity_(max_capacity) {
    if (max_capacity_ == 0) {
        throw std::invalid_argument("Cache capacity must be greater than zero");
    }
}

void KeyValueStore::touch_key(StoreIterator it) {
    lru_keys_.splice(lru_keys_.end(), lru_keys_, it->second.lru_position);
    it->second.lru_position = std::prev(lru_keys_.end());
}

void KeyValueStore::evict_if_needed() {
    while (data_.size() > max_capacity_) {
        auto oldest = data_.find(lru_keys_.front());
        if (oldest != data_.end()) {
            data_.erase(oldest);
        }
        lru_keys_.pop_front();
    }
}

void KeyValueStore::set(const std::string& key, const std::string& value) {
    std::scoped_lock lock(mutex_);
    remove_if_expired(key);

    auto it = data_.find(key);
    if (it != data_.end()) {
        it->second.value = value;
        it->second.expires_at = std::nullopt;
        touch_key(it);
        return;
    }

    lru_keys_.push_back(key);
    auto position = std::prev(lru_keys_.end());
    try {
        data_.emplace(key, StoredValue{value, std::nullopt, position});
    } catch (...) {
        lru_keys_.pop_back();
        throw;
    }
    evict_if_needed();
}

std::optional<std::string> KeyValueStore::get(const std::string& key) {
    std::scoped_lock lock(mutex_);
    remove_if_expired(key);

    auto it = data_.find(key);
    if (it == data_.end()) {
        return std::nullopt;
    }

    touch_key(it);
    return it->second.value;
}

bool KeyValueStore::del(const std::string& key) {
    std::scoped_lock lock(mutex_);
    remove_if_expired(key);

    auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }

    lru_keys_.erase(it->second.lru_position);
    data_.erase(it);
    return true;
}

bool KeyValueStore::exists(const std::string& key) {
    std::scoped_lock lock(mutex_);
    remove_if_expired(key);

    auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }

    touch_key(it);
    return true;
}

bool KeyValueStore::incr(const std::string& key, long long& result) {
    std::scoped_lock lock(mutex_);
    remove_if_expired(key);

    auto it = data_.find(key);
    if (it == data_.end()) {
        result = 1;
        lru_keys_.push_back(key);
        auto position = std::prev(lru_keys_.end());
        try {
            data_.emplace(key, StoredValue{"1", std::nullopt, position});
        } catch (...) {
            lru_keys_.pop_back();
            throw;
        }
        evict_if_needed();
        return true;
    }

    long long current = 0;
    if (!parse_integer(it->second.value, current) ||
        current == std::numeric_limits<long long>::max()) {
        return false;
    }

    result = current + 1;
    it->second.value = std::to_string(result);
    touch_key(it);
    return true;
}

bool KeyValueStore::expire(
    const std::string& key, std::chrono::seconds seconds) {
    std::scoped_lock lock(mutex_);
    remove_if_expired(key);

    auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }

    if (seconds <= std::chrono::seconds::zero()) {
        lru_keys_.erase(it->second.lru_position);
        data_.erase(it);
        return true;
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

    if (remaining.count() <= 0) {
        lru_keys_.erase(it->second.lru_position);
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
        lru_keys_.erase(it->second.lru_position);
        data_.erase(it);
    }
}

bool KeyValueStore::parse_integer(
    const std::string& text, long long& value) const {
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
