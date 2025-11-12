#include "keyword_recommender.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <queue>
#include <vector>

namespace {

size_t editDistance(const std::string &a, const std::string &b) {
    const size_t n = a.size();
    const size_t m = b.size();
    if (n == 0) return m;
    if (m == 0) return n;

    std::vector<size_t> prev(m + 1), curr(m + 1);
    for (size_t j = 0; j <= m; ++j) prev[j] = j;

    for (size_t i = 1; i <= n; ++i) {
        curr[0] = i;
        for (size_t j = 1; j <= m; ++j) {
            size_t cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            curr[j] = std::min({ prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost });
        }
        std::swap(prev, curr);
    }
    return prev[m];
}

struct SuggestionNode {
    KeywordSuggestion suggestion;
};

struct SuggestionCompare {
    bool operator()(const SuggestionNode &a, const SuggestionNode &b) const {
        if (a.suggestion.distance != b.suggestion.distance) {
            return a.suggestion.distance < b.suggestion.distance;
        }
        if (a.suggestion.frequency != b.suggestion.frequency) {
            return a.suggestion.frequency > b.suggestion.frequency;
        }
        return a.suggestion.word < b.suggestion.word;
    }
};

} // namespace

bool loadKeywordDictFile(const std::string &dict_path,
                         std::vector<std::string> &words,
                         std::vector<uint32_t> &frequencies) {
    std::ifstream fin(dict_path);
    if (!fin) return false;
    words.clear();
    frequencies.clear();
    std::string line;
    while (std::getline(fin, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string word;
        uint32_t freq = 0;
        if (!(iss >> word >> freq)) continue;
        words.push_back(std::move(word));
        frequencies.push_back(freq);
    }
    return !words.empty();
}

std::vector<KeywordSuggestion> recommendKeywords(const std::string &input,
                                                 const std::vector<std::string> &words,
                                                 const std::vector<uint32_t> &frequencies,
                                                 size_t topK) {
    std::vector<KeywordSuggestion> result;
    if (input.empty() || words.empty() || topK == 0) return result;

    // 第一阶段：收集前缀匹配的词
    std::vector<KeywordSuggestion> prefix_matches;
    for (size_t i = 0; i < words.size(); ++i) {
        if (words[i].find(input) == 0) {  // 前缀匹配
            prefix_matches.push_back({words[i], frequencies[i], 0});
        }
    }
    
    // 如果有足够的前缀匹配，优先返回它们
    if (prefix_matches.size() >= topK) {
        // 按频次排序
        std::sort(prefix_matches.begin(), prefix_matches.end(), 
                  [](const KeywordSuggestion &a, const KeywordSuggestion &b){
            if (a.frequency != b.frequency) return a.frequency > b.frequency;
            return a.word < b.word;
        });
        result.assign(prefix_matches.begin(), prefix_matches.begin() + topK);
        return result;
    }
    
    // 如果前缀匹配不足，使用编辑距离补充
    std::priority_queue<SuggestionNode, std::vector<SuggestionNode>, SuggestionCompare> heap;
    
    // 先加入前缀匹配的结果
    for (const auto &match : prefix_matches) {
        heap.push({match});
    }
    
    // 再补充编辑距离近的词
    for (size_t i = 0; i < words.size(); ++i) {
        // 跳过已经在前缀匹配中的词
        if (words[i].find(input) == 0) continue;
        
        size_t dist = editDistance(input, words[i]);
        
        // 如果包含输入（但不是前缀），给予优惠
        if (words[i].find(input) != std::string::npos) {
            dist = dist / 2;
        }
        
        SuggestionNode node{ { words[i], frequencies[i], dist } };
        heap.push(node);
        if (heap.size() > topK) {
            heap.pop();
        }
    }

    result.reserve(heap.size());
    while (!heap.empty()) {
        result.push_back(heap.top().suggestion);
        heap.pop();
    }
    std::sort(result.begin(), result.end(), [](const KeywordSuggestion &a, const KeywordSuggestion &b){
        if (a.distance != b.distance) return a.distance < b.distance;
        if (a.frequency != b.frequency) return a.frequency > b.frequency;
        return a.word < b.word;
    });
    return result;
}


