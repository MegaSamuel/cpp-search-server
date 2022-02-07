#include <iostream>
#include <cmath>
#include <numeric>

#include "search_server.h"
#include "string_processing.h"

using namespace std;

SearchServer::SearchServer(const string& text) {
    // Наличие спецсимволов — то есть символов с кодами в диапазоне от 0 до 31 включительно
    if(!IsValidWord(text))
        throw invalid_argument("Forbidden symbol is detected in stop-words"s);

    for(const string& word : SplitIntoWords(text)) {
        stop_words_.insert(word);
    }
}

void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
    // Наличие спецсимволов — то есть символов с кодами в диапазоне от 0 до 31 включительно
    if(!IsValidWord(document))
        throw invalid_argument("Forbidden symbol is detected"s);

    // Попытка добавить документ с отрицательным id
    if(document_id < 0)
        throw invalid_argument("Id is negative"s);

    // Попытка добавить документ с id, совпадающим с id документа, который добавился ранее
    if(documents_.count(document_id))
        throw invalid_argument("Document already exists"s);

    const vector<string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for(const string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
    }

    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query, DocumentStatus status) const {
    return SearchServer::FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) { return document_status == status; });
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query) const {
    return SearchServer::FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

int SearchServer::GetDocumentId(int number) const {
    if((number < 0) || (number >= GetDocumentCount())) {
        throw out_of_range("Number "s + to_string(number) + " is out of range"s);
    }

    auto it = documents_.begin();

    advance(it, number);

    return it->first;
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {
    const Query query = ParseQuery(raw_query);

    vector<string> matched_words;

    for(const string& word : query.plus_words) {
        if(word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if(word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    for(const string& word : query.minus_words) {
        if(word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if(word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }

    tuple<vector<string>, DocumentStatus> result;
    result = {matched_words, documents_.at(document_id).status};

    return result;
}

bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}
    
vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
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
    
SearchServer::QueryWord SearchServer::ParseQueryWord(string text) const {
    bool is_minus = false;

    // Word shouldn't be empty
    if(text[0] == '-') {
        // после '-' нет букв
        if(1 == text.length())
            throw invalid_argument("Detected no letters after '-' symbol"s);

        // несколько подряд символов '-'
        if('-' == text[1])
            throw invalid_argument("Detected several '-' symbols in a row in \""s + text + "\""s);

        is_minus = true;
        text = text.substr(1);
    }

    // проверка на наличие спецсимволов
    if(!IsValidWord(text))
        throw invalid_argument("Forbidden symbol is detected in \""s + text + "\""s);

    return {text, is_minus, IsStopWord(text)};
}
    
SearchServer::Query SearchServer::ParseQuery(const string& text) const {
    SearchServer::Query query;

    for(const string& word : SplitIntoWords(text)) {
        const SearchServer::QueryWord query_word = ParseQueryWord(word);
        if(!query_word.is_stop) {
            if(query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            } else {
                query.plus_words.insert(query_word.data);
            }
        }
    }

    return query;
}
    
// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

bool SearchServer::IsValidWord(const string& word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {return c >= '\0' && c < ' ';});
}

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
                 const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const exception& e) {
        cout << "Add document error "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout << "Search result: "s << raw_query << endl;
    try {
        for(const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    } catch (const exception& e) {
        cout << "Search error: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "Match result: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for(int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const exception& e) {
        cout << "Match error "s << query << ": "s << e.what() << endl;
    }
}
