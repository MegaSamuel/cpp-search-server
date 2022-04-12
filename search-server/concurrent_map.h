#pragma once

#include <map>
#include <mutex>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {

struct t_item {
    std::mutex mutex; // мутекс
    std::map<Key, Value> map; // мапа
};

public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        std::lock_guard<std::mutex> guard; // защита от одновременного доступа

        Value& ref_to_value; // ссылка на значение
    };

    explicit ConcurrentMap(size_t bucket_count) : m_vct_items(bucket_count) {
    }

    // ! используем простое число для количества элементов
    explicit ConcurrentMap() : ConcurrentMap(31) {
    }

    Access operator[](const Key& key) {
        // остаток от деления ключа на размер дает индекс
        auto& item = m_vct_items[static_cast<uint64_t>(key) % m_vct_items.size()];

        return {std::lock_guard<std::mutex>(item.mutex), item.map[key]};
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;

        // проход по всем кускам мапы
        for(auto& item : m_vct_items) {
            // сначала блокируем
            std::lock_guard guard(item.mutex);
            // потом вставляем
            result.insert(item.map.begin(), item.map.end());
        }

        return result;
    }

private:
    // вектор элементов
    std::vector<t_item> m_vct_items;
};
