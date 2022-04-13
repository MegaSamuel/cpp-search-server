#pragma once

#include <execution>
#include <algorithm>
#include <stdexcept>
#include <map>
#include <set>

#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"
//#include "log_duration.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:
    // Defines an invalid document id
    inline static constexpr int INVALID_DOCUMENT_ID = -1;
    // Defines an error for double values
    inline static constexpr double EPSILON_DOUBLE = 1e-6;

    explicit SearchServer() = default;

    explicit SearchServer(const std::string& text) : SearchServer(SplitIntoWords(text)) {};

    explicit SearchServer(const std::string_view text) : SearchServer(SplitIntoWords(text)) {};

    template <typename StringCollection>
    explicit SearchServer(const StringCollection& stop_words);

    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view raw_query) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, const std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, const std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, const std::string_view raw_query) const;

    int GetDocumentCount() const;

    // константные итераторы на начало и конец множества с id документов
    std::set<int>::const_iterator begin() const;
    std::set<int>::const_iterator end() const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);
    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);
    void RemoveDocument(const std::execution::parallel_policy&, int document_id);

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy&, const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy&, const std::string_view raw_query, int document_id) const;

    // мапа: ключ - id документа, значение - множество слов
    // need for RemoveDuplicates()
    std::map<int, std::set<std::string>> document_to_set_words;

private:
    struct DocumentData {
        int rating; // рейтинг
        DocumentStatus status; // статус
        std::string text; // текст
    };
    
    // множество стоп-слов
    std::set<std::string, std::less<>> stop_words_;
    // мапа: ключ - ссылка на слово, значение - мапа: ключ - id документа, значение - частота
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    // мапа: ключ - id документа, значение - данные документа
    std::map<int, DocumentData> documents_;
    // множество из id добавленных документов
    std::set<int> documents_id_;
    // мапа: ключ - id документа, значение - мапа: ключ - ссылка на слово, значение - частота
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;

    bool IsStopWord(const std::string_view word) const;
    
    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;
    
    static int ComputeAverageRating(const std::vector<int>& ratings);
    
    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };
    
    QueryWord ParseQueryWord(const std::string_view text) const;
    
    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };
    
    Query ParseQuery(const std::string_view text) const;
    Query ParseQuery(const std::execution::sequenced_policy&, const std::string_view text) const;
    Query ParseQuery(const std::execution::parallel_policy&, const std::string_view text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const;

    static bool IsValidWord(const std::string_view word);
};

template <typename StringCollection>
SearchServer::SearchServer(const StringCollection& stop_words) {
    for(const auto& it : stop_words) {
        // Наличие спецсимволов — то есть символов с кодами в диапазоне от 0 до 31 включительно
        if(!IsValidWord(it))
            throw std::invalid_argument("Forbidden symbol is detected in stop-word \"" + static_cast<std::string>(it) + "\"");

        // если слово it не пустое, то добавляем
        // stop_words_ это множество set, соответственно дубликаты отсеются
        if(!it.empty())
            stop_words_.insert(static_cast<std::string>(it));
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const {
    const Query query = ParseQuery(raw_query);

    std::vector<Document> result = FindAllDocuments(query, document_predicate);
        
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
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(raw_query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, const std::string_view raw_query, DocumentPredicate document_predicate) const {
    const Query query = ParseQuery(raw_query);

    std::vector<Document> result = FindAllDocuments(std::execution::par, query, document_predicate);

    sort(std::execution::par, result.begin(), result.end(),
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
    for(const std::string_view& word : query.plus_words) {
        if(word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for(const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if(document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for(const std::string_view& word : query.minus_words) {
        if(word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for(const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
            (void)_; // убираем предупреждение об неиспользуемой переменной
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    matched_documents.reserve(document_to_relevance.size());
    for(const auto& [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({
            document_id,
            relevance,
            documents_.at(document_id).rating
        });
    }

    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy&, const SearchServer::Query& query, DocumentPredicate document_predicate) const {
    return FindAllDocuments(query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy&, const SearchServer::Query& query, DocumentPredicate document_predicate) const {
        ConcurrentMap<int, double> concurrent_document_to_relevance;

        // проход по плюс словам
        for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(),
            [this, &document_predicate, &concurrent_document_to_relevance](const std::string_view word) {
                if(word_to_document_freqs_.count(word) == 0) {
                    return;
                }
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                for(const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                    const auto& document_data = documents_.at(document_id);
                    if(document_predicate(document_id, document_data.status, document_data.rating)) {
                        concurrent_document_to_relevance[document_id] += term_freq * inverse_document_freq;
                    }
                }
            });

        // проход по минус словам
        for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(),
            [this, &concurrent_document_to_relevance](const std::string_view word) {
                if(word_to_document_freqs_.count(word) == 0) {
                    return;
                }
                for(const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
                    (void)_;
                    concurrent_document_to_relevance.erase(document_id);
                }
            });

        std::map<int, double> document_to_relevance = concurrent_document_to_relevance.BuildOrdinaryMap();

        std::vector<Document> matched_documents(document_to_relevance.size());

        transform(std::execution::par, document_to_relevance.begin(), document_to_relevance.end(), matched_documents.begin(),
            [this](const auto& it) {
                return Document(it.first, it.second, documents_.at(it.first).rating);
            });

        return matched_documents;
}
