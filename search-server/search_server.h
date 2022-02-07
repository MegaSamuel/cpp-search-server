#pragma once

#include <string>
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
    explicit SearchServer(const StringCollection& stop_words) {
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

    void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const {
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

    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(const std::string& raw_query) const;

    int GetDocumentCount() const;

    int GetDocumentId(int number) const;

    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;

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
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        std::map<int, double> document_to_relevance;
        for(const std::string& word : query.plus_words) {
            if(word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
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

    static bool IsValidWord(const std::string& word);
};
