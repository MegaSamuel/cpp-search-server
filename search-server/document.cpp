#include <iostream>
#include <string>
#include <stdexcept>

#include "document.h"

using namespace std::string_literals;

Document::Document() : id(0), relevance(0), rating(0) {

}

Document::Document(int _id, double _relevance, int _rating) : id(_id), relevance(_relevance), rating(_rating) {
    
}

// вывод Document
std::ostream& operator<<(std::ostream& output, const Document& document) { 
    output << "{ "s
           << "document_id = "s << document.id << ", "s
           << "relevance = "s << document.relevance << ", "s
           << "rating = "s << document.rating << " }"s;
    return output;
}
