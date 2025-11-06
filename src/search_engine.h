#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include "weighted_inverted_index.h"

struct SearchResult {
    int docid;
    std::string title;
    std::string link;
    std::string summary; // 根据查询词自动抽取
    double score;        // 余弦相似度
};

class SearchEngine {
public:
    SearchEngine(const WeightedInvertedIndex &index,
                 const std::string &pages_path,
                 const std::string &offsets_path);

    bool loadOffsets();

    // 基于 AND + 余弦相似度的查询，返回按得分降序的结果
    std::vector<SearchResult> queryRanked(const std::vector<std::string> &terms, size_t top_k = 20);

private:
    struct RawPage { std::string title, link, description; };
    bool readPageByOffset(std::streampos offset, RawPage &out);
    bool readPageByDocId(int docid, RawPage &out);
    static bool extractTag(const std::string &xml, const std::string &tag, std::string &out);
    static std::string makeSummary(const std::string &text, const std::vector<std::string> &terms, size_t window = 120);
    static std::string escapeJson(const std::string &s);

    const WeightedInvertedIndex &index;
    std::string pages_path;
    std::string offsets_path;
    std::unordered_map<int, std::streampos> docid_to_offset;
};


