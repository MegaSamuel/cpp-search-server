#include <iostream>
#include <string>
#include <stdexcept>

#include "document.h"

using namespace std;

Document::Document() : id(0), relevance(0), rating(0) {

}

Document::Document(int _id, double _relevance, int _rating) : id(_id), relevance(_relevance), rating(_rating) {
    
}

// вывод Document
ostream& operator<<(ostream& output, const Document& document) { 
    output << "{ "s
           << "document_id = "s << document.id << ", "s
           << "relevance = "s << document.relevance << ", "s
           << "rating = "s << document.rating << " }"s;
    return output;
}

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
    for(const string& word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}
