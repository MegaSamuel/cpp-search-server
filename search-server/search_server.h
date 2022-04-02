#pragma once

#include <execution>
//#include <iostream> // for debug print
#include <string>
#include <string_view>
#include <algorithm>
#include <stdexcept>
#include <map>
#include <set>

#include "document.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:
    // Defines an invalid document id
    inline static constexpr int INVALID_DOCUMENT_ID = -1;
    // Defines an error for double values
    inline static constexpr double EPSILON_DOUBLE = 1e-6;

    explicit SearchServer() = default;

    explicit SearchServer(const std::string& text);

    template <typename StringCollection>
    explicit SearchServer(const StringCollection& stop_words);

    void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(const std::string& raw_query) const;

    int GetDocumentCount() const;

    // константные итераторы на начало и конец множества с id документов
    std::set<int>::const_iterator begin() const;
    std::set<int>::const_iterator end() const;

    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    template <typename ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy, int document_id);
 
    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;

    // мапа: ключ - id документа, значение - множество слов
    // need for RemoveDuplicates()
    std::map<int, std::set<std::string>> document_to_set_words;

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    
    // множество стоп-слов
    std::set<std::string> stop_words_;
    // мапа: ключ - слово, значение - мапа: ключ - id документа, значение - частота
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    // мапа: ключ - id документа, значение - данные документа
    std::map<int, DocumentData> documents_;
    // множество из id добавленных документов
    std::set<int> documents_id_;
    // мапа: ключ - id документа, значение - мапа: ключ - слово, значение - частота
    std::map<int, std::map<std::string, double>> document_to_word_freqs_;

    bool IsStopWord(const std::string& word) const;
    
    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;
    
    static int ComputeAverageRating(const std::vector<int>& ratings);
    
    struct QueryWord {
        std::string data;
        bool is_minus;
        bool is_stop;
    };
    
    QueryWord ParseQueryWord(std::string text) const;
    
    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };
    
    Query ParseQuery(const std::string& text) const;
    
    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;

    static bool IsValidWord(const std::string& word);
};

template <typename StringCollection>
SearchServer::SearchServer(const StringCollection& stop_words) {
    for(auto& it : stop_words) {
        // Наличие спецсимволов — то есть символов с кодами в диапазоне от 0 до 31 включительно
        if(!IsValidWord(it))
            throw std::invalid_argument("Forbidden symbol is detected in stop-word \"" + it + "\"");

        // если слово it не пустое, то добавляем
        // stop_words_ это множество set, соответственно дубликаты отсеются
        if(!it.empty())
            stop_words_.insert(it);
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const {
    const Query query = SearchServer::ParseQuery(raw_query);

    std::vector<Document> result = SearchServer::FindAllDocuments(query, document_predicate);
        
    sort(result.begin(), result.end(),
         [](const Document& lhs, const Document& rhs) {
            if(std::abs(lhs.relevance - rhs.relevance) < EPSILON_DOUBLE) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }
         });
    if(result.size() > MAX_RESULT_DOCUMENT_COUNT) {
        result.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return result;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const SearchServer::Query& query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for(const std::string& word : query.plus_words) {
        if(word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = SearchServer::ComputeWordInverseDocumentFreq(word);
        for(const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if(document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }
        
    for(const std::string& word : query.minus_words) {
        if(word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for(const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            (void)_; // убираем предупреждение об неиспользуемой переменной
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for(const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({
            document_id,
            relevance,
            documents_.at(document_id).rating
        });
    }

    return matched_documents;
}

template <typename ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy, int document_id) {

    //std::cout << "mark 1\n";

	if constexpr (std::is_same_v<ExecutionPolicy, std::execution::sequenced_policy>) {
        //std::cout << "mark 2\n";
        RemoveDocument(document_id);
        // // есть ли такой документ?
        // auto it_document_id = find(std::execution::seq, documents_id_.begin(), documents_id_.end(), document_id);
        // if(it_document_id == documents_id_.end()) {
        //     return;
        // }

        // // ссылка на мапу
        // const auto& map_word_freq = document_to_word_freqs_.at(document_id);
    
        // // вспомогательный вектор указателей на слова
        // std::vector<const std::string*> vct_words_ptr(map_word_freq.size());
    
        // // заполняем вектор
        // std::transform(std::execution::seq, map_word_freq.begin(), map_word_freq.end(), vct_words_ptr.begin(),
        //     [](const auto& pair_word_freq) {
        //         return &pair_word_freq.first;
        //     });

        // // удаляем документ с document_id из всех приватных структур
        // std::for_each(std::execution::seq, vct_words_ptr.begin(), vct_words_ptr.end(),
        //     [this, document_id](const std::string* word_ptr) {
        //         word_to_document_freqs_.at(*word_ptr).erase(document_id);
        //     });

        // documents_.erase(document_id);
        // documents_id_.erase(it_document_id);
        // document_to_word_freqs_.erase(document_id);

        // // удаляем документ с document_id из всех публичных структур
        // // document_to_set_words.erase(document_id);
	}

    if constexpr (std::is_same_v<ExecutionPolicy, std::execution::parallel_policy>) {
        //std::cout << "mark 3\n";
        // есть ли такой документ?
        if(!documents_id_.count(document_id)) {
            return;
        }
        // auto it_document_id = find(std::execution::par, documents_id_.begin(), documents_id_.end(), document_id);
        // if(it_document_id == documents_id_.end()) {
        //     return;
        // }

        // ссылка на мапу
        const auto& map_word_freq = document_to_word_freqs_.at(document_id);
    
        // вспомогательный вектор слов
        std::vector<const std::string*> vct_words(map_word_freq.size());
    
        // заполняем вектор
        std::transform(std::execution::par, map_word_freq.begin(), map_word_freq.end(), vct_words.begin(),
            [](const auto& pair_word_freq) {
                return &pair_word_freq.first;
            });

        // удаляем документ с document_id из всех приватных структур
        std::for_each(std::execution::par, vct_words.begin(), vct_words.end(),
            [this, document_id](const std::string* word) {
                word_to_document_freqs_.at(*word).erase(document_id);
            });

        documents_.erase(document_id);
        documents_id_.erase(document_id);
        document_to_word_freqs_.erase(document_id);

        // удаляем документ с document_id из всех публичных структур
        // document_to_set_words.erase(document_id);
    }
}
