#pragma once

#include <algorithm>
#include <cstdlib>
#include <execution>
#include <map>
#include <mutex>
#include <set>
#include <vector>

template <typename Key, typename Value>
struct Mutex_Map {
    std::map<Key, Value>     data;
    std::mutex               mutex_data;
};

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
        std::lock_guard<std::mutex>     value_guard;
        Value&                          ref_to_value;
    };

    explicit                                      ConcurrentMap(size_t bucket_count = 1);

    Access                                        operator[](const Key& key); 
    
    auto                                          Erase(const Key& key);

    std::map<Key, Value>                          BuildOrdinaryMap();

private:
    std::vector<Mutex_Map<Key, Value>>            mutex_maps_;
};

template <typename Key, typename Value>
ConcurrentMap<Key, Value>::ConcurrentMap(size_t bucket_count) 
    : mutex_maps_(bucket_count) {}

template <typename Key, typename Value>
typename ConcurrentMap<Key, Value>::Access ConcurrentMap<Key, Value>::operator[](const Key& key) {
    uint64_t tmp_key = static_cast<uint64_t>(key) % mutex_maps_.size();
    return {std::lock_guard(mutex_maps_[tmp_key].mutex_data), mutex_maps_[tmp_key].data[key]};
}

template <typename Key, typename Value>
auto ConcurrentMap<Key, Value>::Erase(const Key& key) {
    uint64_t tmp_key = static_cast<uint64_t>(key) % mutex_maps_.size();
    std::lock_guard guard(mutex_maps_[tmp_key].mutex_data);
    return mutex_maps_[tmp_key].data.erase(key);
}

template <typename Key, typename Value>
std::map<Key, Value> ConcurrentMap<Key, Value>::BuildOrdinaryMap() {
    std::map<Key, Value> main_map;
    std::for_each(mutex_maps_.begin(), mutex_maps_.end(), 
            [&main_map] (auto& part_map) {
                std::lock_guard<std::mutex> guard(part_map.mutex_data);
                main_map.insert(part_map.data.begin(), part_map.data.end());
            }
    );
    return main_map;
}