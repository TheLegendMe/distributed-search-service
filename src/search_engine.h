#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <memory>
#include "weighted_inverted_index.h"

struct SearchResult {
    int docid;
    std::string title;
    std::string link;
    std::string summary; // 根据查询词自动抽取
    double score;        // 余弦相似度
};

class SearchCache;

class SearchEngine {
public:
    SearchEngine(const WeightedInvertedIndex &index,
                 const std::string &pages_path,
                 const std::string &offsets_path);
    ~SearchEngine();

    bool loadOffsets();
    
    // 启用缓存
    void enableCache(const std::string &redis_host = "127.0.0.1",
                     int redis_port = 6379,
                     size_t local_capacity = 1000,
                     int cache_ttl = 3600);
    
    // 获取缓存统计
    void getCacheStats(size_t &local_hits, size_t &redis_hits, size_t &misses, size_t &local_size);
    
    // 清空缓存
    void clearCache();

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
    
    // 双层缓存
    std::unique_ptr<SearchCache> cache_;
    
    // 生成缓存 key
    std::string makeCacheKey(const std::vector<std::string> &terms, size_t top_k);
};


