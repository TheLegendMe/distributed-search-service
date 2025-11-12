#include "search_engine.h"
#include "search_cache.h"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <chrono>

namespace {
    // 快速清理非法 UTF-8（只处理一次，避免重复检查）
    inline std::string cleanUtf8Fast(const std::string &str) {
        // 快速路径：如果全是 ASCII 直接返回
        bool has_multibyte = false;
        for (unsigned char c : str) {
            if (c >= 0x80) {
                has_multibyte = true;
                break;
            }
        }
        if (!has_multibyte) return str;
        
        // 慢速路径：有多字节字符才处理
        std::string result;
        result.reserve(str.size());
        for (size_t i = 0; i < str.size(); ) {
            unsigned char c = static_cast<unsigned char>(str[i]);
            if (c < 0x80) {
                result.push_back(str[i++]);
            } else if ((c & 0xE0) == 0xC0 && i + 1 < str.size() &&
                       (str[i+1] & 0xC0) == 0x80) {
                result.append(str, i, 2);
                i += 2;
            } else if ((c & 0xF0) == 0xE0 && i + 2 < str.size() &&
                       (str[i+1] & 0xC0) == 0x80 && (str[i+2] & 0xC0) == 0x80) {
                result.append(str, i, 3);
                i += 3;
            } else if ((c & 0xF8) == 0xF0 && i + 3 < str.size() &&
                       (str[i+1] & 0xC0) == 0x80 && (str[i+2] & 0xC0) == 0x80 && (str[i+3] & 0xC0) == 0x80) {
                result.append(str, i, 4);
                i += 4;
            } else {
                i++;  // 跳过非法字节
            }
        }
        return result;
    }
}

SearchEngine::SearchEngine(const WeightedInvertedIndex &idx,
                           const std::string &pages,
                           const std::string &offsets)
    : index(idx), pages_path(pages), offsets_path(offsets), cache_(nullptr) {}

SearchEngine::~SearchEngine() = default;

bool SearchEngine::loadOffsets() {
    std::ifstream fin(offsets_path);
    if (!fin) return false;
    docid_to_offset.clear();
    int id; long long off;
    while (fin >> id >> off) {
        docid_to_offset[id] = static_cast<std::streampos>(off);
    }
    return !docid_to_offset.empty();
}

bool SearchEngine::extractTag(const std::string &xml, const std::string &tag, std::string &out) {
    std::string open = "<" + tag + ">";
    std::string close = "</" + tag + ">";
    auto pos1 = xml.find(open);
    if (pos1 == std::string::npos) return false;
    pos1 += open.size();
    auto pos2 = xml.find(close, pos1);
    if (pos2 == std::string::npos) return false;
    out = xml.substr(pos1, pos2 - pos1);
    return true;
}

bool SearchEngine::readPageByOffset(std::streampos offset, RawPage &out) {
    std::ifstream fin(pages_path);
    if (!fin) return false;
    fin.seekg(offset);
    std::string block;
    block.reserve(2048);
    std::string line;
    while (std::getline(fin, line)) {
        block += line;
        block.push_back('\n');
        if (line == "</doc>") break;
    }
    if (block.empty()) return false;
    extractTag(block, "title", out.title);
    extractTag(block, "link", out.link);
    extractTag(block, "description", out.description);
    return true;
}

bool SearchEngine::readPageByDocId(int docid, RawPage &out) {
    auto it = docid_to_offset.find(docid);
    if (it == docid_to_offset.end()) return false;
    return readPageByOffset(it->second, out);
}

std::string SearchEngine::makeSummary(const std::string &text, const std::vector<std::string> &terms, size_t window) {
    if (text.empty()) return "";
    std::string lower = text;
    for (char &c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    size_t pos = std::string::npos;
    for (const auto &t : terms) {
        std::string tl = t; for (char &c : tl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        size_t p = lower.find(tl);
        if (p != std::string::npos) { if (pos == std::string::npos || p < pos) pos = p; }
    }
    if (pos == std::string::npos) {
        if (text.size() <= window) return text;
        return text.substr(0, window) + "...";
    }
    size_t start = (pos > window/2) ? pos - window/2 : 0;
    size_t end = std::min(text.size(), start + window);
    return (start ? "..." : "") + text.substr(start, end - start) + (end < text.size() ? "..." : "");
}

std::string SearchEngine::escapeJson(const std::string &s) {
    std::string out; out.reserve(s.size());
    for (char ch : s) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

void SearchEngine::enableCache(const std::string &redis_host,
                               int redis_port,
                               size_t local_capacity,
                               int cache_ttl) {
    cache_ = std::make_unique<SearchCache>(redis_host, redis_port, local_capacity, cache_ttl);
}

void SearchEngine::getCacheStats(size_t &local_hits, size_t &redis_hits, size_t &misses, size_t &local_size) {
    if (cache_) {
        auto stats = cache_->getStats();
        local_hits = stats.local_hits;
        redis_hits = stats.redis_hits;
        misses = stats.misses;
        local_size = stats.local_size;
    } else {
        local_hits = redis_hits = misses = local_size = 0;
    }
}

void SearchEngine::clearCache() {
    if (cache_) {
        cache_->clear();
    }
}

std::string SearchEngine::makeCacheKey(const std::vector<std::string> &terms, size_t top_k) {
    std::ostringstream oss;
    for (size_t i = 0; i < terms.size(); ++i) {
        if (i > 0) oss << " ";
        oss << terms[i];
    }
    oss << "|" << top_k;
    return oss.str();
}

std::vector<SearchResult> SearchEngine::queryRanked(const std::vector<std::string> &terms, size_t top_k) {
    std::vector<SearchResult> results;
    auto start_time = std::chrono::steady_clock::now();
    
    // 构建查询字符串用于日志
    std::string query_str;
    for (size_t i = 0; i < terms.size(); ++i) {
        if (i > 0) query_str += " ";
        query_str += terms[i];
    }
    
    // 如果启用了缓存，先查缓存
    if (cache_) {
        std::string cache_key = makeCacheKey(terms, top_k);
        
        // 记录缓存查询前的统计
        size_t local_hits_before, redis_hits_before, misses_before, local_size;
        getCacheStats(local_hits_before, redis_hits_before, misses_before, local_size);
        
        if (cache_->get(cache_key, results)) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
            
            // 判断是本地缓存还是 Redis 缓存命中
            size_t local_hits_after, redis_hits_after, misses_after, _;
            getCacheStats(local_hits_after, redis_hits_after, misses_after, _);
            
            if (local_hits_after > local_hits_before) {
                std::cout << "[CACHE HIT - LOCAL] Query: \"" << query_str 
                          << "\" | Results: " << results.size() 
                          << " | Time: " << duration << "μs" << std::endl;
            } else {
                std::cout << "[CACHE HIT - REDIS] Query: \"" << query_str 
                          << "\" | Results: " << results.size() 
                          << " | Time: " << duration << "μs" << std::endl;
            }
            return results;
        }
        
        std::cout << "[CACHE MISS] Query: \"" << query_str 
                  << "\" | Searching..." << std::endl;
    }
    
    // 缓存未命中或未启用缓存，执行实际搜索
    auto ranked = index.searchANDCosineRanked(terms);
    if (ranked.empty()) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        std::cout << "[SEARCH] Query: \"" << query_str 
                  << "\" | Results: 0 | Time: " << duration << "ms" << std::endl;
        return results;
    }
    
    if (top_k && ranked.size() > top_k) ranked.resize(top_k);

    for (const auto &pr : ranked) {
        RawPage pg;
        if (!readPageByDocId(pr.first, pg)) continue;
        SearchResult r;
        r.docid = pr.first;
        r.title = cleanUtf8Fast(pg.title);
        r.link = cleanUtf8Fast(pg.link);
        r.summary = cleanUtf8Fast(makeSummary(pg.description, terms));
        r.score = pr.second;
        results.emplace_back(std::move(r));
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    std::cout << "[SEARCH] Query: \"" << query_str 
              << "\" | Results: " << results.size() 
              << " | Time: " << duration << "ms";
    
    // 将结果存入缓存
    if (cache_ && !results.empty()) {
        std::string cache_key = makeCacheKey(terms, top_k);
        cache_->put(cache_key, results);
        std::cout << " | Cached: Yes";
    }
    std::cout << std::endl;
    
    return results;
}


