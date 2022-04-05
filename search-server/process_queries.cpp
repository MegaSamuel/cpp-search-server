#include <algorithm>
#include <execution>

#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server,
                                                  const std::vector<std::string>& queries) {
    // создаем вектор нужного размера
    std::vector<std::vector<Document>> documents_lists(queries.size());

    // заполняем вектор
    std::transform(std::execution::par, queries.begin(), queries.end(), documents_lists.begin(),
                   [&search_server](const std::string& query) { return search_server.FindTopDocuments(query); });

    return documents_lists;
}

std::list<Document> ProcessQueriesJoined(const SearchServer& search_server,
                                           const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> documents_lists(queries.size());

    documents_lists = ProcessQueries(search_server, queries);

    std::list<Document> documents;

    for(auto it = documents_lists.begin(); it != documents_lists.end(); it++) {
        for(auto iit = it->begin(); iit != it->end(); iit++) {
            documents.push_back(std::move(*iit));
        }
    }

    return documents;
}
