#include "search_cache.h"
#include <sstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

SearchCache::SearchCache(const std::string &redis_host,
                         int redis_port,
                         size_t local_capacity,
                         int cache_ttl)
    : redis_ctx_(nullptr),
      redis_host_(redis_host),
      redis_port_(redis_port),
      cache_ttl_(cache_ttl),
      local_capacity_(local_capacity),
      local_hits_(0),
      redis_hits_(0),
      misses_(0) {
    connectRedis();
}

SearchCache::~SearchCache() {
    disconnectRedis();
}

bool SearchCache::connectRedis() {
    struct timeval timeout = { 2, 0 };  // 2 秒超时
    redis_ctx_ = redisConnectWithTimeout(redis_host_.c_str(), redis_port_, timeout);
    
    if (redis_ctx_ == nullptr || redis_ctx_->err) {
        if (redis_ctx_) {
            std::cerr << "Redis connection error: " << redis_ctx_->errstr << std::endl;
            redisFree(redis_ctx_);
            redis_ctx_ = nullptr;
        } else {
            std::cerr << "Redis connection error: can't allocate redis context" << std::endl;
        }
        return false;
    }
    
    // 测试连接
    redisReply *reply = (redisReply*)redisCommand(redis_ctx_, "PING");
    if (reply == nullptr) {
        std::cerr << "Redis PING failed" << std::endl;
        redisFree(redis_ctx_);
        redis_ctx_ = nullptr;
        return false;
    }
    freeReplyObject(reply);
    
    std::cout << "Connected to Redis at " << redis_host_ << ":" << redis_port_ << std::endl;
    return true;
}

void SearchCache::disconnectRedis() {
    if (redis_ctx_) {
        redisFree(redis_ctx_);
        redis_ctx_ = nullptr;
    }
}

bool SearchCache::get(const std::string &query, std::vector<SearchResult> &results) {
    // 1. 先查本地 LRU 缓存
    if (getFromLocal(query, results)) {
        std::lock_guard<std::mutex> lock(mutex_);
        local_hits_++;
        return true;
    }
    
    // 2. 再查 Redis 缓存
    if (getFromRedis(query, results)) {
        // 命中后更新本地 LRU
        putToLocal(query, results);
        std::lock_guard<std::mutex> lock(mutex_);
        redis_hits_++;
        return true;
    }
    
    // 3. 缓存未命中
    {
        std::lock_guard<std::mutex> lock(mutex_);
        misses_++;
    }
    return false;
}

void SearchCache::put(const std::string &query, const std::vector<SearchResult> &results) {
    // 同时更新本地和 Redis 缓存
    putToLocal(query, results);
    putToRedis(query, results);
}

bool SearchCache::getFromLocal(const std::string &key, std::vector<SearchResult> &results) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = lru_map_.find(key);
    if (it == lru_map_.end()) {
        return false;
    }
    
    // 命中，移到链表头部（最近使用）
    results = it->second->value;
    lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
    return true;
}

void SearchCache::putToLocal(const std::string &key, const std::vector<SearchResult> &results) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = lru_map_.find(key);
    if (it != lru_map_.end()) {
        // 已存在，更新并移到头部
        it->second->value = results;
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
        return;
    }
    
    // 不存在，检查容量
    if (lru_list_.size() >= local_capacity_) {
        // 淘汰最久未使用的（链表尾部）
        auto last = lru_list_.back();
        lru_map_.erase(last.key);
        lru_list_.pop_back();
    }
    
    // 插入到头部
    lru_list_.push_front({key, results});
    lru_map_[key] = lru_list_.begin();
}

bool SearchCache::getFromRedis(const std::string &key, std::vector<SearchResult> &results) {
    if (!redis_ctx_) {
        return false;
    }
    
    std::string cache_key = "search:" + key;
    redisReply *reply = (redisReply*)redisCommand(redis_ctx_, "GET %s", cache_key.c_str());
    
    if (reply == nullptr) {
        // 连接断开，尝试重连
        disconnectRedis();
        connectRedis();
        return false;
    }
    
    bool success = false;
    if (reply->type == REDIS_REPLY_STRING) {
        std::string data(reply->str, reply->len);
        success = deserializeResults(data, results);
    }
    
    freeReplyObject(reply);
    return success;
}

void SearchCache::putToRedis(const std::string &key, const std::vector<SearchResult> &results) {
    if (!redis_ctx_) {
        return;
    }
    
    std::string cache_key = "search:" + key;
    std::string data = serializeResults(results);
    
    // 序列化失败，跳过缓存
    if (data.empty()) {
        return;
    }
    
    redisReply *reply = (redisReply*)redisCommand(redis_ctx_, 
                                                   "SETEX %s %d %b",
                                                   cache_key.c_str(),
                                                   cache_ttl_,
                                                   data.c_str(),
                                                   data.size());
    
    if (reply == nullptr) {
        // 连接断开，尝试重连
        disconnectRedis();
        connectRedis();
        return;
    }
    
    freeReplyObject(reply);
}

std::string SearchCache::serializeResults(const std::vector<SearchResult> &results) {
    // 数据已在 SearchEngine 层清理，直接序列化
    // 使用 try-catch 作为最后防线
    try {
        json j = json::array();
        
        for (const auto &r : results) {
            json item;
            item["docid"] = r.docid;
            item["title"] = r.title;
            item["link"] = r.link;
            item["summary"] = r.summary;
            item["score"] = r.score;
            j.push_back(item);
        }
        
        return j.dump();
    } catch (const std::exception &e) {
        // JSON 序列化失败，返回空（不缓存该结果）
        std::cerr << "JSON serialization error: " << e.what() << std::endl;
        return "";
    }
}

bool SearchCache::deserializeResults(const std::string &data, std::vector<SearchResult> &results) {
    try {
        json j = json::parse(data);
        results.clear();
        
        for (const auto &item : j) {
            SearchResult r;
            r.docid = item["docid"];
            r.title = item["title"];
            r.link = item["link"];
            r.summary = item["summary"];
            r.score = item["score"];
            results.push_back(r);
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

SearchCache::Stats SearchCache::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {
        local_hits_,
        redis_hits_,
        misses_,
        lru_list_.size()
    };
}

void SearchCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    lru_list_.clear();
    lru_map_.clear();
    
    if (redis_ctx_) {
        // 清空 Redis 中的搜索缓存（通过模式删除）
        redisReply *reply = (redisReply*)redisCommand(redis_ctx_, "KEYS search:*");
        if (reply && reply->type == REDIS_REPLY_ARRAY) {
            for (size_t i = 0; i < reply->elements; i++) {
                redisCommand(redis_ctx_, "DEL %s", reply->element[i]->str);
            }
        }
        if (reply) freeReplyObject(reply);
    }
}

