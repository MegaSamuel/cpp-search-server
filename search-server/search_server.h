#pragma once

#include <execution>
#include <algorithm>
#include <stdexcept>
#include <map>
#include <set>

#include "document.h"
#include "string_processing.h"

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

    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query, DocumentPredicate document_predicate) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query, DocumentStatus status) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;

    int GetDocumentCount() const;

    // константные итераторы на начало и конец множества с id документов
    std::set<int>::const_iterator begin() const;
    std::set<int>::const_iterator end() const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    template <typename ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int document_id);
 
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;

    template <typename ExecutionPolicy>
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(ExecutionPolicy&& policy, const std::string_view raw_query, int document_id) const;

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

    template <typename ExecutionPolicy>
    Query ParseQuery(ExecutionPolicy&& policy, const std::string_view text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(ExecutionPolicy&& policy, const Query& query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;

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

template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query, DocumentPredicate document_predicate) const {
	if constexpr (std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::sequenced_policy>) {
        return FindTopDocuments(raw_query, document_predicate);
	}

    if constexpr (std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::parallel_policy>) {
        const Query query = ParseQuery(policy, raw_query);

        std::vector<Document> result = FindAllDocuments(policy, query, document_predicate);
        
        sort(policy, result.begin(), result.end(),
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

    return {};
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query, 
        [status]
        (int document_id, DocumentStatus document_status, int rating) 
        {(void)document_id; (void)rating; return document_status == status; });
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
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

template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy&& policy, const SearchServer::Query& query, DocumentPredicate document_predicate) const {
	if constexpr (std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::sequenced_policy>) {
        return FindAllDocuments(query, document_predicate);
	}

    if constexpr (std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::parallel_policy>) {
        (void)policy;

        std::map<int, double> document_to_relevance;
        for(const std::string_view& word : query.plus_words) {
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
        
        for(const std::string_view& word : query.minus_words) {
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

    return {};
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const SearchServer::Query& query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for(const std::string_view& word : query.plus_words) {
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
        
    for(const std::string_view& word : query.minus_words) {
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
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id) {
	if constexpr (std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::sequenced_policy>) {
        RemoveDocument(document_id);
	}

    if constexpr (std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::parallel_policy>) {
        // есть ли такой документ?
        if(!documents_id_.count(document_id)) {
            return;
        }

        // ссылка на мапу
        const auto& map_word_freq = document_to_word_freqs_.at(document_id);
    
        // вспомогательный вектор слов
        std::vector<const std::string*> vct_words(map_word_freq.size());
    
        // заполняем вектор
        transform(policy, map_word_freq.begin(), map_word_freq.end(), vct_words.begin(),
            [](const auto& pair_word_freq) {
                return &pair_word_freq.first;
            });

        // удаляем документ с document_id из всех приватных структур
        for_each(policy, vct_words.begin(), vct_words.end(),
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

template <typename ExecutionPolicy>
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(ExecutionPolicy&& policy, const std::string_view raw_query, int document_id) const {
	if constexpr (std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::sequenced_policy>) {
        return MatchDocument(raw_query, document_id);
	}

    if constexpr (std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::parallel_policy>) {
        const Query query = ParseQuery(policy, raw_query);

        // ссылка на мапу
        const auto& map_word_freq = document_to_word_freqs_.at(document_id);

        // проход по минус словам
        if(any_of(policy, query.minus_words.begin(), query.minus_words.end(),
            [this, &map_word_freq](const std::string_view& word) {
                return map_word_freq.count(word); })) {
            return {std::vector<std::string_view>{}, documents_.at(document_id).status};
        }

        // вектор максимально возможного размера
        std::vector<std::string_view> matched_words(query.plus_words.size());

        // проход по плюс словам
        auto it = copy_if(policy, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(),
            [this, &map_word_freq](const std::string_view& word) {
                return map_word_freq.count(word);
        });
 
        // слова лежат в векторе - сортируем
        std::sort(policy, matched_words.begin(), matched_words.end());
 
        // оставляем только уникальные слова
        auto iit = std::unique(policy, matched_words.begin(), matched_words.end());

        return {{matched_words.begin(), next(iit, -1)}, documents_.at(document_id).status};
    }

    return {std::vector<std::string_view>{}, documents_.at(document_id).status};
}

template <typename ExecutionPolicy>
SearchServer::Query SearchServer::ParseQuery(ExecutionPolicy&& policy, const std::string_view text) const {
    (void)policy;

	if constexpr (std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::sequenced_policy>) {
        return ParseQuery(text);
	}

    if constexpr (std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::parallel_policy>) {
        Query query;

        for(const std::string_view& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if(!query_word.is_stop) {
                query_word.is_minus ? 
                query.minus_words.push_back(query_word.data) : 
                query.plus_words.push_back(query_word.data);
            }
        }

        return query;
    }

    return {};
}
