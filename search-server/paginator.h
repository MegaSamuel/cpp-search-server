#pragma once

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
        Iterator it_begin = m_it_begin;

        //TODO как-то криво это выглядит, надо бы переписать
        while(it_begin != m_it_end)
        {
            Iterator it_end = it_begin; 
            int dist1 = distance(it_end, m_it_end);
            if(dist1 < m_page_size)
                advance(it_end, dist1);
            else
                advance(it_end, m_page_size);

            IteratorRange<Iterator> it_range(it_begin, it_end);
            m_vctRange.push_back(it_range);

            int dist2 = distance(it_begin, m_it_end);
            if(dist2 < m_page_size)
                advance(it_begin, dist2);
            else
                advance(it_begin, m_page_size);
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
    //output << endl;
    return output;
}

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}
