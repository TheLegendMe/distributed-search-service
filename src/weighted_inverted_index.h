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

    // 余弦相似度排序（AND 语义）：
    // 1) 将查询词当作文档，计算其 TF-IDF 权重向量 X
    // 2) 仅保留包含全部查询词的文档，取每个文档中各查询词对应的权重向量 Y
    // 3) 计算 cos = (X·Y)/(|X||Y|)，按 cos 降序排序
    std::vector<std::pair<int, double>> searchANDCosineRanked(const std::vector<std::string> &terms) const;

    // 文档总数（用于计算 IDF）
    size_t docCount() const { return total_docs; }

    // 只读访问，供持久化输出使用
    const InvertIndexTable &data() const { return postings; }
    // 从文本索引加载（格式同 output/index.txt），并设置 total_docs
    bool loadFromFile(const std::string &index_path, size_t total_docs_count);

private:
    InvertIndexTable postings;
    size_t total_docs = 0;
};


