#pragma once

#include <string>
#include <vector>

struct Document {
    int id;
    double relevance;
    int rating;

    Document();
    Document(int _id, double _relevance, int _rating);
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

// вывод Document
std::ostream& operator<<(std::ostream& output, const Document& document);
