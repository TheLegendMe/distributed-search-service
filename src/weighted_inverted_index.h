#pragma once
#include <unordered_map>
#include <set>
#include <vector>
#include <string>
#include <mutex>

// TF-IDF 加权倒排索引
// postings: term -> [(docId, weight)]，其中 weight 为 TF-IDF 权重
// InvertIndexTable 定义：term -> set<(docId, weight)>
// set 默认按 pair<int,double> 的字典序排序（先 docId，后 weight），保证去重
using InvertIndexTable = std::unordered_map<std::string, std::set<std::pair<int, double>>>;

class WeightedInvertedIndex {
public:
    // 输入：文档集合，每个元素 pair<docId, 文本>
    void build(const std::vector<std::pair<int, std::string>> &documents);

    // 查询：返回按权重和降序的 docId 简单列表（演示用）
    std::vector<int> searchAND(const std::vector<std::string> &terms) const;

    // 加权查询：
    // - AND：仅包含所有查询词的文档，得分为各词在该文档权重之和，按降序
    // - OR：包含任意查询词的文档，得分为各词在该文档权重之和，按降序
    std::vector<int> searchANDWeighted(const std::vector<std::string> &terms) const;
    std::vector<int> searchORWeighted(const std::vector<std::string> &terms) const;

    // 只读访问，供持久化输出使用
    const InvertIndexTable &data() const { return postings; }

private:
    InvertIndexTable postings;
};


