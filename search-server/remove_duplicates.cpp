#include <iostream>
#include <string>
#include <vector>
#include <set>

#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    // сложность O(wN(logN+logW))

    std::vector<int> document_for_erase;
    std::set<std::set<std::string>> set_of_set_words;

    for(const int document_id : search_server) {
        // ссылка на множество слов документа document_id
        const auto& set_words = search_server.document_to_set_words.at(document_id);
        if(0 == set_of_set_words.count(set_words)) {
            // если множество слов не существует - запоминаем его
            set_of_set_words.insert(set_words);
        } else {
            // если такое множество слов уже существует, то помечаем документ к удалению
            document_for_erase.push_back(document_id);
            // и сообщаем об этом
            std::cout << "Found duplicate document id " << document_id << std::endl;
        }
    }

    // собственно даление документов
    for(const int document_id : document_for_erase) {
        search_server.RemoveDocument(document_id);
    }
}
