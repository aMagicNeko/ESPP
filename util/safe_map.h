#pragma once

#include <unordered_map>
#include <atomic>
#include <bthread/bthread.h>
#include <functional>

template < typename K, typename V, size_t ParN = 1, 
         typename MapType = std::unordered_map<K, V>,
         typename LockType = bthread::Mutex >
class SafeMap {
public:
    SafeMap() : _size(0) {}
    ~SafeMap() {}

    bool insert(const K &key, const V &value) {
        uint32_t par_id = get_par_id(key);
        std::lock_guard<LockType> lck(_locks[par_id]);
        std::pair<typename MapType::iterator, bool> ret;
        ret = _internal_storages[par_id].insert(std::make_pair(key, value));
        if (true == ret.second) {
            _size++;
        }
        return ret.second;
    }

    void set_value(const K &key, const V &value) {
        uint32_t par_id = get_par_id(key);
        std::lock_guard<LockType> lck(_locks[par_id]);

        typename MapType::iterator it = _internal_storages[par_id].find(key);
        if (_internal_storages[par_id].end() == it) {
            _size++;
        }
        _internal_storages[par_id][key] = value;
    }

    bool get_value(const K &key, V &value) {
        uint32_t par_id = get_par_id(key);
        std::lock_guard<LockType> lck(_locks[par_id]);
        typename MapType::iterator it = _internal_storages[par_id].find(key);
        if (_internal_storages[par_id].end() == it) {
            return false;
        }
        value = it->second;
        return true;
    }

    template <typename _Storage>
    void get_storages(_Storage &value) {
        for (size_t par_id = 0; par_id < ParN; par_id++) {
            std::lock_guard<LockType> lck(_locks[par_id]);
            value.insert(_internal_storages[par_id].begin(), _internal_storages[par_id].end());
        }
    }

    bool exists(const K &key) {
        uint32_t par_id = get_par_id(key);
        std::lock_guard<LockType> lck(_locks[par_id]);
        typename MapType::iterator it = _internal_storages[par_id].find(key);
        if (_internal_storages[par_id].end() == it) {
            return false;
        }
        return true;
    }

    bool erase(const K &key) {
        uint32_t par_id = get_par_id(key);
        std::lock_guard<LockType> lck(_locks[par_id]);
        typename MapType::iterator it = _internal_storages[par_id].find(key);
        if (_internal_storages[par_id].end() == it) {
            return false;
        }
        _internal_storages[par_id].erase(it);
        _size--;
        return true;
    }

    template <typename _Predicate>
    bool erase_if(const K &key, _Predicate pred) {
        uint32_t par_id = get_par_id(key);
        std::lock_guard<LockType> lck(_locks[par_id]);
        typename MapType::iterator it = _internal_storages[par_id].find(key);
        if (_internal_storages[par_id].end() == it) {
            return false;
        }
        if (!pred(it->second)) {
            return false;
        }
        _internal_storages[par_id].erase(it);
        _size--;
        return true;
    }

    void clear() {
        for (size_t par_id = 0; par_id < ParN; par_id++) {
            std::lock_guard<LockType> lck(_locks[par_id]);
            _internal_storages[par_id].clear();
        }
    }

    void get_keys(std::vector<K> &vec) {
        for (size_t par_id = 0; par_id < ParN; par_id++) {
            std::lock_guard<LockType> lck(_locks[par_id]);
            for (const auto & pair : _internal_storages[par_id]) {
                vec.push_back(pair.first);
            }
        }
    }

    size_t size() const {
        return _size;
    }

private:
    uint32_t get_par_id(const K &key) {
        std::hash<K> hash_fn;
        uint32_t par_id = hash_fn(key) % ParN;
        return par_id;
    }

private:
    MapType _internal_storages[ParN];
    LockType _locks[ParN];
    std::atomic<size_t> _size;
};