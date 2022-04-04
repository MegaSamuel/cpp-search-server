#include "string_processing.h"

std::vector<std::string> SplitIntoWords(const std::string& text) {
    std::vector<std::string> words;
    std::string word;

    for(const char c : text) {
        if(c == ' ') {
            if(!word.empty()) {
                // добавляем слово только если оно не пустое
                words.push_back(word);
                word = "";
            }
        } else {
            word += c;
        }
    }
    words.push_back(word);

    return words;
}

std::vector<std::string_view> SplitIntoWords_v(std::string_view text) {
    // реализацию взяли из 7-го спринта
    std::vector<std::string_view> words;
    const int64_t pos_end = text.npos;
    text.remove_prefix(0);
    while(true) {
        while((!text.empty()) && (' ' == text.front())) {
            text.remove_prefix(1);
        }
        int64_t space = text.find(' ', 0);
        words.push_back(space == pos_end ? text.substr(0) : text.substr(0, space));
        if(space == pos_end) {
            break;
        } else {
            text.remove_prefix(space+1);
        }
    }
    return words;
}
