#include "key_value_store.hpp"

#include <cassert>
#include <chrono>
#include <thread>
#include <vector>

void test_store_sets_and_gets_value() {
    KeyValueStore store;

    store.set("name", "Slava");

    auto value = store.get("name");
    assert(value.has_value());
    assert(*value == "Slava");
}

void test_store_reports_missing_value() {
    KeyValueStore store;

    auto value = store.get("missing");

    assert(!value.has_value());
}

void test_store_deletes_existing_key() {
    KeyValueStore store;
    store.set("name", "Slava");

    bool deleted = store.del("name");

    assert(deleted);
    assert(!store.exists("name"));
}

void test_store_does_not_delete_missing_key() {
    KeyValueStore store;

    bool deleted = store.del("missing");

    assert(!deleted);
}

void test_store_increments_missing_key() {
    KeyValueStore store;
    long long result = 0;

    bool incremented = store.incr("counter", result);

    assert(incremented);
    assert(result == 1);
    assert(store.get("counter") == "1");
}

void test_store_increments_existing_integer() {
    KeyValueStore store;
    long long result = 0;

    store.set("counter", "41");
    bool incremented = store.incr("counter", result);

    assert(incremented);
    assert(result == 42);
    assert(store.get("counter") == "42");
}

void test_store_rejects_non_integer_increment() {
    KeyValueStore store;
    long long result = 0;

    store.set("name", "Slava");
    bool incremented = store.incr("name", result);

    assert(!incremented);
    assert(store.get("name") == "Slava");
}

void test_store_expire_existing_key() {
    KeyValueStore store;

    store.set("name", "Slava");
    bool updated = store.expire("name", std::chrono::seconds(10));

    assert(updated);
    assert(store.ttl("name") >= 0);
}

void test_store_expire_missing_key() {
    KeyValueStore store;

    bool updated = store.expire("missing", std::chrono::seconds(10));

    assert(!updated);
}

void test_store_ttl_missing_key() {
    KeyValueStore store;

    assert(store.ttl("missing") == -2);
}

void test_store_ttl_key_without_expiration() {
    KeyValueStore store;

    store.set("name", "Slava");

    assert(store.ttl("name") == -1);
}

void test_store_treats_expired_key_as_missing() {
    KeyValueStore store;

    store.set("name", "Slava");
    assert(store.expire("name", std::chrono::seconds(1)));

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    assert(!store.exists("name"));
    assert(!store.get("name").has_value());
    assert(store.ttl("name") == -2);
}

void test_store_supports_concurrent_increments() {
    KeyValueStore store;
    constexpr int thread_count = 4;
    constexpr int increments_per_thread = 250;

    std::vector<std::thread> threads;
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([&store]() {
            for (int j = 0; j < increments_per_thread; ++j) {
                long long result = 0;
                bool incremented = store.incr("counter", result);
                assert(incremented);
            }
        });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    auto value = store.get("counter");
    assert(value.has_value());
    assert(*value == std::to_string(thread_count * increments_per_thread));
}

void run_key_value_store_tests() {
    test_store_sets_and_gets_value();
    test_store_reports_missing_value();
    test_store_deletes_existing_key();
    test_store_does_not_delete_missing_key();
    test_store_increments_missing_key();
    test_store_increments_existing_integer();
    test_store_rejects_non_integer_increment();
    test_store_expire_existing_key();
    test_store_expire_missing_key();
    test_store_ttl_missing_key();
    test_store_ttl_key_without_expiration();
    test_store_treats_expired_key_as_missing();
    test_store_supports_concurrent_increments();
}
