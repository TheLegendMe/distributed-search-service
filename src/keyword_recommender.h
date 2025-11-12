#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct KeywordSuggestion {
    std::string word;
    uint32_t frequency;
    size_t distance;
};

bool loadKeywordDictFile(const std::string &dict_path,
                         std::vector<std::string> &words,
                         std::vector<uint32_t> &frequencies);

std::vector<KeywordSuggestion> recommendKeywords(const std::string &input,
                                                 const std::vector<std::string> &words,
                                                 const std::vector<uint32_t> &frequencies,
                                                 size_t topK);


