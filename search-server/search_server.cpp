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

    for(const string_view word : SplitIntoWords(text)) {
        stop_words_.insert(static_cast<string>(word));
    }
}

SearchServer::SearchServer(string_view text) {
    // Наличие спецсимволов — то есть символов с кодами в диапазоне от 0 до 31 включительно
    if(!IsValidWord(text))
        throw invalid_argument("Forbidden symbol is detected in stop-words"s);

    for(const string_view word : SplitIntoWords(text)) {
        stop_words_.insert(static_cast<string>(word));
    }
}

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

    // добавляем в вектор id документа
    documents_id_.push_back(document_id);

    const vector<string_view> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for(const string_view word : words) {
        // преврящаем string_view в string
        string str{word};

        // формируем мапу по слову
        word_to_document_freqs_[str][document_id] += inv_word_count;

        // формируем мапу по id
        //document_to_word_freqs_[document_id][word] += inv_word_count;
        // во второй мапе теперь не string а string_view !!
        document_to_word_freqs_[document_id][word_to_document_freqs_.find(str)->first] += inv_word_count;

        // формируем мапу с множеством по id
        document_to_set_words[document_id].insert(str);
    }

    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
}

vector<Document> SearchServer::FindTopDocuments(const string_view raw_query, DocumentStatus status) const {
    return SearchServer::FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) { (void)document_id; (void)rating; return document_status == status; });
}

vector<Document> SearchServer::FindTopDocuments(const string_view raw_query) const {
    return SearchServer::FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

vector<int>::const_iterator SearchServer::begin() const {
    // константная сложность
    return documents_id_.begin();
}

vector<int>::const_iterator SearchServer::end() const {
    // константная сложность
    return documents_id_.end();
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    // сложность O(logN)

    if(0 == document_to_word_freqs_.count(document_id)) {
        // результат объявляем как статик иначе вернем ссылку на локальный объект
        static const map<string_view, double> res;
        // возвращаем пустой результат
        return res;
    }
    return document_to_word_freqs_.at(document_id);
}

// ---> remove document

void SearchServer::RemoveDocument(const execution::sequenced_policy&, int document_id) {
    // есть ли такой документ?
    auto it = document_to_word_freqs_.find(document_id);

    // если есть
    if(it != document_to_word_freqs_.end()) {
        // удаляем документ с document_id из всех приватных структур
        for_each(it->second.begin(), it->second.end(),
            [this, document_id](pair<const string_view, double>& pair_word_freq) {
                // превращаем string_view в string
                string word = static_cast<string>(pair_word_freq.first);
                word_to_document_freqs_.at(word).erase(document_id);
                // если по этому слову нет записей
                if(word_to_document_freqs_.at(word).empty()) {
                    // удаляем запись
                    word_to_document_freqs_.erase(word);
                } });

        documents_.erase(document_id);
        documents_id_.erase(find(documents_id_.begin(), documents_id_.end(), document_id));
        document_to_word_freqs_.erase(document_id);

        // удаляем документ с document_id из всех публичных структур
        document_to_set_words.erase(document_id);
    }
}

void SearchServer::RemoveDocument(const execution::parallel_policy&, int document_id) {
    // есть ли такой документ?
    if(0 == document_to_word_freqs_.count(document_id)) {
        return;
    }

    // ссылка на мапу
    const auto& map_word_freq = document_to_word_freqs_.at(document_id);
    
    // вспомогательный вектор слов
    vector<string_view> words(map_word_freq.size());
    
    // заполняем вектор
    transform(execution::par, map_word_freq.begin(), map_word_freq.end(), words.begin(),
        [](const auto& item) { return item.first; });

    // удаляем документ с document_id из всех приватных структур
    for_each(execution::par, words.begin(), words.end(),
        [this, document_id](string_view& word) {
            word_to_document_freqs_.at(static_cast<string>(word)).erase(document_id);
        });

    documents_.erase(document_id);
    documents_id_.erase(find(documents_id_.begin(), documents_id_.end(), document_id));
    document_to_word_freqs_.erase(document_id);

    // удаляем документ с document_id из всех публичных структур
    document_to_set_words.erase(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    // вызываем многопоточную версию с признаком однопоточности
    RemoveDocument(execution::seq, document_id);
}

// <--- remove document

// ---> match document

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const execution::sequenced_policy&, const string_view raw_query, int document_id) const {
    const Query query = ParseQuery(raw_query);

    vector<string> matched_words;

    // проход по плюс словам
    for_each(execution::seq, query.plus_words.begin(), query.plus_words.end(),
        [this, document_id, &matched_words](const string& word) {
            if(word_to_document_freqs_.count(word)) {
                if(word_to_document_freqs_.at(word).count(document_id)) {
                    matched_words.push_back(word);
                }
            } 
        });

    // проход по минус словам
    for_each(execution::seq, query.minus_words.begin(), query.minus_words.end(),
        [this, document_id, &matched_words](const string& word) {
            if(word_to_document_freqs_.count(word)) {
                if(word_to_document_freqs_.at(word).count(document_id)) {
                    matched_words.clear();
                }
            }
        });

    return {matched_words, documents_.at(document_id).status};
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const execution::parallel_policy&, const string_view raw_query, int document_id) const {
    const Query query = ParseQuery(raw_query);

    set<string_view> set_matched_words;

    // ссылка на мапу
    const auto& map_word_freq = document_to_word_freqs_.at(document_id);

    // проход по минус словам
    for(const string_view word : query.minus_words) {
        if(map_word_freq.count(word)) {
            // пустой результат
            return {vector<string>{}, documents_.at(document_id).status};
        }
    }

    // проход по плюс словам
    for_each(execution::par, query.plus_words.begin(), query.plus_words.end(),
        [document_id, &set_matched_words, map_word_freq](const string_view word){
            // ищем слово
            auto it = map_word_freq.find(word);
            if(it != map_word_freq.end()) {
                // если нашли - добавляем
                set_matched_words.insert(it->first);
            }
        }
    );

    // формирукм вектор для результата
    vector<string> matched_words;
    matched_words.reserve(set_matched_words.size());
    for(const string_view word : set_matched_words) {
        matched_words.push_back(static_cast<string>(word));
    }

    // результат
    return {matched_words, documents_.at(document_id).status};
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string_view raw_query, int document_id) const {
    // вызываем многопоточную версию с признаком однопоточности
    return MatchDocument(execution::seq, raw_query, document_id);
}

// <--- match document

bool SearchServer::IsStopWord(const string_view word) const {
    return stop_words_.count(static_cast<string>(word)) > 0;
}
    
vector<string_view> SearchServer::SplitIntoWordsNoStop(const string_view text) const {
    vector<string_view> words;
    for (const string_view word : SplitIntoWords(text)) {
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
    
SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
    bool is_minus = false;

    // Word shouldn't be empty
    if(text[0] == '-') {
        // после '-' нет букв
        if(1 == text.length())
            throw invalid_argument("Detected no letters after '-' symbol"s);

        // несколько подряд символов '-'
        if('-' == text[1])
            throw invalid_argument("Detected several '-' symbols in a row in \""s + static_cast<string>(text) + "\""s);

        is_minus = true;
        text = text.substr(1);
    }

    // проверка на наличие спецсимволов
    if(!IsValidWord(text))
        throw invalid_argument("Forbidden symbol is detected in \""s + static_cast<string>(text) + "\""s);

    return {text, is_minus, IsStopWord(text)};
}
    
SearchServer::Query SearchServer::ParseQuery(const string_view text) const {
    SearchServer::Query query;

    for(const string_view word : SplitIntoWords(text)) {
        const SearchServer::QueryWord query_word = ParseQueryWord(word);
        if(!query_word.is_stop) {
            if(query_word.is_minus) {
                query.minus_words.insert(static_cast<string>(query_word.data));
            } else {
                query.plus_words.insert(static_cast<string>(query_word.data));
            }
        }
    }

    return query;
}
    
// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

bool SearchServer::IsValidWord(const string_view& word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {return c >= '\0' && c < ' ';});
}
