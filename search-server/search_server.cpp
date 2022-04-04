#include <iostream>
#include <cmath>
#include <numeric>

#include "search_server.h"

using namespace std;

void SearchServer::AddDocument(int document_id, const string_view document, DocumentStatus status, const vector<int>& ratings) {
    // Наличие спецсимволов — то есть символов с кодами в диапазоне от 0 до 31 включительно
    if(!IsValidWord(document))
        throw invalid_argument("Forbidden symbol is detected"s);

    // Попытка добавить документ с отрицательным id
    if(document_id < 0)
        throw invalid_argument("Id is negative"s);

    // Попытка добавить документ с id, совпадающим с id документа, который добавился ранее
    if(documents_.count(document_id))
        throw invalid_argument("Document already exists"s);

    // добавляем в множество id документа
    documents_id_.insert(document_id);

    // в DocumentData теперь кладем и сам текст документа, во всех других местах будут ссылки на него
    // emplace вернет пару: итератор, bool
    const auto [it, is_inserted] = documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status, static_cast<string>(document)});

    const vector<string_view> words = SplitIntoWordsNoStop(it->second.text);

    const double inv_word_count = 1.0 / words.size();
    for(const string_view& word : words) {
        // формируем мапу по слову
        word_to_document_freqs_[word][document_id] += inv_word_count;

        // формируем мапу по id
        document_to_word_freqs_[document_id][word] += inv_word_count;

        // формируем мапу с множеством по id
        // document_to_set_words[document_id].insert(static_cast<string>(word));
    }
}

vector<Document> SearchServer::FindTopDocuments(const string_view raw_query, DocumentStatus status) const {
    return SearchServer::FindTopDocuments(raw_query, 
        [status]
        (int document_id, DocumentStatus document_status, int rating) 
        {(void)document_id; (void)rating; return document_status == status; });
}

vector<Document> SearchServer::FindTopDocuments(const string_view raw_query) const {
    return SearchServer::FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::set<int>::const_iterator SearchServer::begin() const {
    // константная сложность
    return documents_id_.begin();
}

std::set<int>::const_iterator SearchServer::end() const {
    // константная сложность
    return documents_id_.end();
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    // сложность O(logN)

    if(0 == document_to_word_freqs_.count(document_id)) {
        // результат объявляем как статик иначе вернем ссылку на локальный объект
        static const std::map<std::string_view, double> res;
        // возвращаем пустой результат
        return res;
    }
    return document_to_word_freqs_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    // есть ли такой документ?
    if(!documents_id_.count(document_id)) {
        return;
    }

    for_each(word_to_document_freqs_.begin(), word_to_document_freqs_.end(),
        [this, document_id](auto& it) {
            it.second.erase(document_id);
        });

    documents_.erase(document_id);
    documents_id_.erase(document_id);
    document_to_word_freqs_.erase(document_id);

    // удаляем документ с document_id из всех публичных структур
    // document_to_set_words.erase(document_id);
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const string_view raw_query, int document_id) const {
    const Query query = ParseQuery(raw_query);

    // проход по минус словам
    for(const string_view& word : query.minus_words) {
        if(word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if(word_to_document_freqs_.at(word).count(document_id)) {
            return {vector<string_view>{}, documents_.at(document_id).status};
        }
    }

    vector<string_view> matched_words;
    matched_words.reserve(query.plus_words.size());

    // проход по плюс словам
    for(const string_view& word : query.plus_words) {
        if(word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if(word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    return {matched_words, documents_.at(document_id).status};
}

bool SearchServer::IsStopWord(const string_view word) const {
    return stop_words_.count(word) > 0;
}
    
vector<string_view> SearchServer::SplitIntoWordsNoStop(const string_view text) const {
    vector<string_view> words;
    for(const string_view& word : SplitIntoWords(text)) {
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}
    
int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
}
    
SearchServer::QueryWord SearchServer::ParseQueryWord(const string_view text) const {
    bool is_minus = false;

    string_view word = text;

    // Word shouldn't be empty
    if(text[0] == '-') {
        // после '-' нет букв
        if(1 == text.length())
            throw invalid_argument("Detected no letters after '-' symbol"s);

        // несколько подряд символов '-'
        if('-' == text[1])
            throw invalid_argument("Detected several '-' symbols in a row in \""s + static_cast<string>(text) + "\""s);

        is_minus = true;
        word = text.substr(1);
    }

    // проверка на наличие спецсимволов
    if(!IsValidWord(text))
        throw invalid_argument("Forbidden symbol is detected in \""s + static_cast<string>(text) + "\""s);

    return {word, is_minus, IsStopWord(word)};
}
    
SearchServer::Query SearchServer::ParseQuery(const string_view text) const {
    SearchServer::Query query;

    for(const string_view& word : SplitIntoWords(text)) {
        const SearchServer::QueryWord query_word = ParseQueryWord(word);
        if(!query_word.is_stop) {
            query_word.is_minus ? 
            // query.minus_words.push_back(query_word.data) : 
            // query.plus_words.push_back(query_word.data);
            query.minus_words.insert(query_word.data) : 
            query.plus_words.insert(query_word.data);
        }
    }

    // // слова лежат в векторе - сортируем
    // sort(query.plus_words.begin(), query.plus_words.end());
    // sort(query.minus_words.begin(), query.minus_words.end());

    // // оставляем только уникальные слова
    // auto it = unique(query.plus_words.begin(), query.plus_words.end());
    // // лишнее отрезаем
    // query.plus_words.resize(distance(query.plus_words.begin(), it));

    // // оставляем только уникальные слова
    // it = unique(query.minus_words.begin(), query.minus_words.end());
    // // лишнее отрезаем
    // query.minus_words.resize(distance(query.minus_words.begin(), it));

    return query;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

bool SearchServer::IsValidWord(const string_view word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {return c >= '\0' && c < ' ';});
}
