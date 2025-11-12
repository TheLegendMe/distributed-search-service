#pragma once
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <mutex>
#include <hiredis/hiredis.h>
#include "search_engine.h"

// 双层缓存：本地 LRU + Redis
class SearchCache {
public:
    SearchCache(const std::string &redis_host = "127.0.0.1",
                int redis_port = 6379,
                size_t local_capacity = 1000,
                int cache_ttl = 3600);
    ~SearchCache();
    
    // 查询缓存，返回是否命中
    bool get(const std::string &query, std::vector<SearchResult> &results);
    
    // 更新缓存
    void put(const std::string &query, const std::vector<SearchResult> &results);
    
    // 统计信息
    struct Stats {
        size_t local_hits;
        size_t redis_hits;
        size_t misses;
        size_t local_size;
    };
    Stats getStats() const;
    
    // 清空缓存
    void clear();
    
private:
    // 本地 LRU 缓存节点
    struct CacheNode {
        std::string key;
        std::vector<SearchResult> value;
    };
    
    // Redis 连接
    redisContext *redis_ctx_;
    std::string redis_host_;
    int redis_port_;
    int cache_ttl_;  // Redis 缓存 TTL（秒）
    
    // 本地 LRU 缓存
    size_t local_capacity_;
    std::list<CacheNode> lru_list_;
    std::unordered_map<std::string, std::list<CacheNode>::iterator> lru_map_;
    mutable std::mutex mutex_;
    
    // 统计
    mutable size_t local_hits_;
    mutable size_t redis_hits_;
    mutable size_t misses_;
    
    // 内部方法
    bool connectRedis();
    void disconnectRedis();
    bool getFromLocal(const std::string &key, std::vector<SearchResult> &results);
    void putToLocal(const std::string &key, const std::vector<SearchResult> &results);
    bool getFromRedis(const std::string &key, std::vector<SearchResult> &results);
    void putToRedis(const std::string &key, const std::vector<SearchResult> &results);
    
    // 序列化/反序列化
    std::string serializeResults(const std::vector<SearchResult> &results);
    bool deserializeResults(const std::string &data, std::vector<SearchResult> &results);
};

