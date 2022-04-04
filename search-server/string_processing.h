#pragma once

#include <string>
#include <vector>
#include <string_view>

std::vector<std::string> SplitIntoWords(const std::string& text);

std::vector<std::string_view> SplitIntoWords_v(std::string_view text);
