#pragma once

#include <string>
#include <vector>

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

// вывод Document
std::ostream& operator<<(std::ostream& output, const Document& document);

void PrintDocument(const Document& document);

void PrintMatchDocumentResult(int document_id, const std::vector<std::string>& words, DocumentStatus status);

class SearchServer;

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status,
                 const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string& query);
