#include <algorithm>
#include <execution>

#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server,
                                                  const std::vector<std::string>& queries) {
#if 0
    // тривиальное решение
    std::vector<std::vector<Document>> documents_lists;

    for (const std::string& query : queries) {
        documents_lists.push_back(search_server.FindTopDocuments(query));
    }
#else
    // создаем вектор нужного размера
    std::vector<std::vector<Document>> documents_lists(queries.size());

    // заполняем вектор
    std::transform(std::execution::par, queries.begin(), queries.end(), documents_lists.begin(),
                   [&search_server](const std::string& query) { return search_server.FindTopDocuments(query); });
#endif

    return documents_lists;
}
