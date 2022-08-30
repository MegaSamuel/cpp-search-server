#pragma once

#include <cassert>
#include <vector>
#include <iterator>

template <typename Iterator>
class IteratorRange {
public:
    explicit IteratorRange() = default;

    explicit IteratorRange(Iterator begin, Iterator end) : m_it_begin(begin), m_it_end(end) {}

    Iterator begin() const {
        return m_it_begin;
    }

    Iterator end() const {
        return m_it_end;
    }

    auto size() const {
        return distance(m_it_begin, m_it_end);
    }

private:
    // итератор на начало
    Iterator m_it_begin;

    // итератор на конец
    Iterator m_it_end;
};

template <typename Iterator>
class Paginator {
public:
    explicit Paginator() = default;

    explicit Paginator(Iterator begin, Iterator end, int size) : m_it_begin(begin), m_it_end(end), m_page_size(size) {
        // тут формируем массив m_vctRange на основе m_it_begin, m_it_end и размера страницы m_page_size

        // проверка, что начало "раньше" конца
        assert(end >= begin);
        // проверка, что размер страницы не ноль
        assert(size != 0);

        // итератор на начало изначально указывает на начало
        Iterator it_begin = m_it_begin;

        // пока итераторы не встретятся
        while(it_begin != m_it_end)
        {
            // новый итератор на конец - это старый итератор на начало
            Iterator it_end = it_begin; 

            // двигаем конечный итератор на то, что меньше: расстояние между самим итератором и концом диапазона
            // или размер страницы
            advance(it_end, std::min(static_cast<int>(distance(it_end, m_it_end)), m_page_size));

            // формируем IteratorRange<Iterator> и кладем в вектор
            IteratorRange<Iterator> it_range(it_begin, it_end);
            m_vctRange.push_back(it_range);

            // двигаем начальный итератор на то, что меньше: расстояние между самим итератором и концом диапазона
            // или размер страницы
            advance(it_begin, std::min(static_cast<int>(distance(it_begin, m_it_end)), m_page_size));
        }
    }

    auto begin() const {
        return m_vctRange.begin();
    }

    auto end() const {
        return m_vctRange.end();
    }

    auto size() const {
        return m_vctRange.size();
    }

private:
    // итератор на начало
    Iterator m_it_begin;

    // итератор на конец
    Iterator m_it_end;

    // размер станицы
    int m_page_size;

    // вектор с классами итераторов
    std::vector<IteratorRange<Iterator>> m_vctRange;
};

// вывод IteratorRange
template <typename Iterator>
std::ostream& operator<<(std::ostream& output, const IteratorRange<Iterator>& range) { 
    for(auto it = range.begin(); it != range.end(); ++it) {
        output << *it;
    }
    return output;
}

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}
