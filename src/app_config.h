#pragma once
#include <string>

// 应用统一配置结构
struct AppConfig {
    // Jieba 分词词典目录
    std::string jieba_dict_dir;
    
    // 离线索引构建配置
    std::string input_dir;           // XML 文件输入目录
    std::string output_dir;          // 索引输出目录
    int simhash_threshold;           // SimHash 去重阈值
    
    // 关键词字典构建配置
    std::string candidates_file;     // 候选词文件或目录
    std::string keyword_output_dir;  // 关键词字典输出目录
    
    // 查询配置
    std::string index_dir;           // 索引文件目录
    size_t default_topk;             // 默认返回结果数
    
    // 关键词推荐配置
    std::string keyword_dict_dir;    // 关键词字典目录
    size_t recommend_topk;           // 默认推荐数量
    
    // Web 服务器配置
    std::string web_host;            // 服务器监听地址
    int web_port;                    // 服务器监听端口
    
    // Redis 缓存配置
    bool enable_cache;               // 是否启用缓存
    std::string redis_host;          // Redis 服务器地址
    int redis_port;                  // Redis 服务器端口
    size_t cache_capacity;           // 本地 LRU 缓存容量
    int cache_ttl;                   // Redis 缓存 TTL（秒）
    
    AppConfig();
};

// 从配置文件加载应用配置
bool loadAppConfig(const std::string &path, AppConfig &cfg);

