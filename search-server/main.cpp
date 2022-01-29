#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iterator>
#include <stdexcept>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) 
{
    vector<string> words;
    string word;
    for(const char c : text) 
    {
        if(c == ' ') 
        {
            if(!word.empty())
            {
                // добавляем слово только если оно не пустое
                words.push_back(word);
                word = "";
            }
        }
        else 
        {
            word += c;
        }
    }

    words.push_back(word);

    return words;
}
    
struct Document {
    int id;
    double relevance;
    int rating;

    Document() : id(0), relevance(0), rating(0) {}
    Document(int _id, double _relevance, int _rating) : id(_id), relevance(_relevance), rating(_rating) {}
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    // Defines an invalid document id
    // You can refer to this constant as SearchServer::INVALID_DOCUMENT_ID
    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    explicit SearchServer(const string& text) 
    {
        // Наличие спецсимволов — то есть символов с кодами в диапазоне от 0 до 31 включительно
        if(!IsValidWord(text))
            throw invalid_argument("Forbidden symbol is detected in stop-words"s);

        for(const string& word : SplitIntoWords(text))
        {
            stop_words_.insert(word);
        }
    }

    template <typename StringCollection>
    explicit SearchServer(const StringCollection& stop_words) 
    {
        for(auto& it : stop_words)
        {
            // Наличие спецсимволов — то есть символов с кодами в диапазоне от 0 до 31 включительно
            if(!IsValidWord(it))
                throw invalid_argument("Forbidden symbol is detected in stop-word"s);

            // если слово it не пустое, то добавляем
            // stop_words_ это множество set, соответственно дубликаты отсеются
            if(!it.empty())
                stop_words_.insert(it);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) 
    {
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
        for (const string& word : words) 
        {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }

        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const 
    {
        Query query;

        if(!ParseQuery(raw_query, query))
            throw invalid_argument("Invalid query is detected"s);

        vector<Document> result;
        result = FindAllDocuments(query, document_predicate);
        
        sort(result.begin(), result.end(),
             [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                } else {
                    return lhs.relevance > rhs.relevance;
                }
             });
        if (result.size() > MAX_RESULT_DOCUMENT_COUNT) {
            result.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return result;
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const 
    {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) { return document_status == status; });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const 
    {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const 
    {
        return documents_.size();
    }

    int GetDocumentId(int number) const 
    {
        if((number < 0) || (number >= GetDocumentCount()))
        {
            //return INVALID_DOCUMENT_ID;
            throw out_of_range("number is out of range"s);
        }

        auto it = documents_.begin();

        advance(it, number);

        return it->first;
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const
    {
        Query query;

        if(!ParseQuery(raw_query, query))
            throw invalid_argument("Invalid query is detected"s);

        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }

        tuple<vector<string>, DocumentStatus> result;
        result = {matched_words, documents_.at(document_id).status};

        return result;
    }
    
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    // множество стоп-слов
    set<string> stop_words_;
    // мапа: ключ - слово, значение - мапа: ключ - id документа, значение - частота
    map<string, map<int, double>> word_to_document_freqs_;
    // мапа: ключ - id документа, значение - данные документа
    map<int, DocumentData> documents_;
    
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }
    
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }
    
    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }
    
    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };
    
    [[nodiscard]] bool ParseQueryWord(string text, QueryWord& query_word) const 
    {
        bool is_minus = false;

        // Word shouldn't be empty
        if(text[0] == '-') 
        {
            // после '-' нет букв
            if(1 == text.length())
                return false;

            // несколько подряд символов '-'
            if('-' == text[1])
                return false;

            is_minus = true;
            text = text.substr(1);
        }

        // проверка на наличие спецсимволов
        if(!IsValidWord(text))
            return false;

        query_word = {text, is_minus, IsStopWord(text)};

        return true;
    }
    
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };
    
    [[nodiscard]] bool ParseQuery(const string& text, Query& query) const 
    {
        bool ret = true;

        for(const string& word : SplitIntoWords(text)) 
        {
            QueryWord query_word;
            
            if(!ParseQueryWord(word, query_word))
            {
                ret = false;
                query.minus_words.clear();
                query.plus_words.clear();
                break;
            }

            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }

        return ret;
    }
    
    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }
        
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                (void)_; // убираем предупреждение об неиспользуемой переменной
                document_to_relevance.erase(document_id);
            }
        }
        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
            });
        }
        return matched_documents;
    }

    static bool IsValidWord(const string& word) 
    {
        // A valid word must not contain special characters
        return none_of(word.begin(), word.end(), [](char c) {return c >= '\0' && c < ' ';});
    }
};

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status) {
    cout << "{ "s
         << "document_id = "s << document_id << ", "s
         << "status = "s << static_cast<int>(status) << ", "s
         << "words ="s;
    for (const string& word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
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
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
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
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const exception& e) {
        cout << "Match error "s << query << ": "s << e.what() << endl;
    }
}

int main() 
{
    SearchServer search_server("и в на"s);

    AddDocument(search_server, 1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    AddDocument(search_server, 1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    AddDocument(search_server, -1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    AddDocument(search_server, 3, "большой пёс скво\x12рец евгений"s, DocumentStatus::ACTUAL, {1, 3, 2});
    AddDocument(search_server, 4, "большой пёс скворец евгений"s, DocumentStatus::ACTUAL, {1, 1, 1});

    FindTopDocuments(search_server, "пушистый -пёс"s);
    FindTopDocuments(search_server, "пушистый --кот"s);
    FindTopDocuments(search_server, "пушистый -"s);

    MatchDocuments(search_server, "пушистый пёс"s);
    MatchDocuments(search_server, "модный -кот"s);
    MatchDocuments(search_server, "модный --пёс"s);
    MatchDocuments(search_server, "пушистый - хвост"s);
}
