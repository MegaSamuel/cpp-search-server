#pragma once

#include <string>
#include <vector>
#include <deque>

#include "document.h"
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        QueryResult result;

        // всего запросов
        ++m_request_total;

        result.search_result = m_search_server.FindTopDocuments(raw_query, document_predicate);

        if(result.search_result.empty()) {
            ++m_request_no_result;
        }

        // если число запросов превысило лимит, то выкидываем самый старый результат
        if(m_request_total > min_in_day_) {
            // забрали самый старый
            QueryResult old_result = requests_.front();
            // убрали из дека
            requests_.pop_front();
            // если это был заброс без результата
            if(old_result.search_result.empty()) {
                --m_request_no_result;
            }
        }

        // добавляем результат в дек
        requests_.push_back(result);

        return result.search_result;
    }

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        //TODO а надо ли тут хранить сам результат, может хранить признак результат есть/нет?
        std::vector<Document> search_result;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;

    const SearchServer& m_search_server; // ссылка на сервер
    int m_request_total;                 // всего запросов
    int m_request_no_result;             // всего запросов без результата
};
