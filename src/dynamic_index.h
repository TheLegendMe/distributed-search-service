#pragma once
#include "weighted_inverted_index.h"
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <unordered_set>

/**
 * 动态倒排索引 - 支持实时增删改
 * 
 * 特性：
 * 1. 支持动态添加/删除文档
 * 2. 自动重新计算TF-IDF
 * 3. 线程安全
 * 4. 支持持久化
 */
class DynamicInvertedIndex {
public:
    DynamicInvertedIndex() = default;
    
    // 文档元数据结构
    struct DocumentMeta {
        std::string title;
        std::string link;
        std::string summary;
        std::string text;  // 完整文本
    };
    
    // 从文件加载基础索引
    bool loadFromFile(const std::string &index_path, size_t total_docs_count);
    
    // 添加单个文档（自动更新IDF）
    void addDocument(int docid, const std::string &text);
    
    // 添加文档（带元数据）
    void addDocument(int docid, const DocumentMeta &meta);
    
    // 批量添加文档
    void addDocuments(const std::vector<std::pair<int, std::string>> &documents);
    
    // 获取文档元数据
    bool getDocumentMeta(int docid, DocumentMeta &meta) const;
    
    // 删除文档（标记删除，不立即重建索引）
    void removeDocument(int docid);
    
    // 更新文档（先删后加）
    void updateDocument(int docid, const std::string &new_text);
    
    // 搜索接口（与原WeightedInvertedIndex兼容）
    std::vector<std::pair<int, double>> searchANDCosineRanked(
        const std::vector<std::string> &terms) const;
    
    // 获取索引统计
    struct Stats {
        size_t total_docs;
        size_t active_docs;      // 未被删除的文档数
        size_t deleted_docs;     // 已删除的文档数
        size_t total_terms;      // 词汇表大小
        size_t pending_updates;  // 待处理的更新数
    };
    Stats getStats() const;
    
    // 持久化到文件
    bool saveToFile(const std::string &index_path) const;
    
    // 清理删除的文档（重建索引）
    void compact();
    
    // 是否需要压缩（删除文档过多时）
    bool needsCompaction() const;
    
private:
    // 分词函数
    std::vector<std::string> tokenize(const std::string &text) const;
    
    // 计算TF-IDF
    void computeTFIDF(int docid, const std::vector<std::string> &tokens);
    
    // 重新计算IDF（添加/删除文档后）
    void recomputeIDF();
    
    // 核心数据结构
    InvertIndexTable postings_;
    std::unordered_set<int> deleted_docs_;  // 已删除的文档ID
    std::unordered_map<int, std::vector<std::string>> doc_tokens_;  // 文档->分词结果（用于更新）
    std::unordered_map<int, DocumentMeta> doc_metadata_;  // 文档元数据
    
    size_t total_docs_ = 0;
    
    mutable std::shared_mutex mutex_;  // 读写锁
};

