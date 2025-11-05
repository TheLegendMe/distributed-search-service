#include "weighted_inverted_index.h"
#include "tokenizer.h"
#include <unordered_map>
#include <algorithm>
#include <cmath>

void WeightedInvertedIndex::build(const std::vector<std::pair<int, std::string>> &documents) {
    postings.clear();
    if (documents.empty()) return;

    // 1) 统计 DF
    std::unordered_map<std::string, int> df;
    df.reserve(documents.size() * 8);
    for (const auto &doc : documents) {
        std::vector<std::string> tokens;
        JiebaTokenizer::instance().tokenize(doc.second, tokens);
        std::sort(tokens.begin(), tokens.end());
        tokens.erase(std::unique(tokens.begin(), tokens.end()), tokens.end());
        for (const auto &t : tokens) df[t] += 1;
    }

    const double N = static_cast<double>(documents.size());

    // 2) 计算每篇文档的 TF 和 TF-IDF
    for (const auto &doc : documents) {
        std::vector<std::string> tokens;
        JiebaTokenizer::instance().tokenize(doc.second, tokens);
        if (tokens.empty()) continue;

        std::unordered_map<std::string, int> tf;
        for (const auto &t : tokens) tf[t] += 1;

        int max_tf = 0;
        for (const auto &kv : tf) if (kv.second > max_tf) max_tf = kv.second;
        if (max_tf == 0) continue;

        // 规范化 TF（常见做法：0.5 + 0.5 * tf/max_tf）
        for (const auto &kv : tf) {
            const std::string &term = kv.first;
            int df_t = df[term];
            double tf_norm = 0.5 + 0.5 * (static_cast<double>(kv.second) / static_cast<double>(max_tf));
            double idf = std::log((N + 1.0) / (static_cast<double>(df_t) + 1.0)) + 1.0;
            double w = tf_norm * idf;
            postings[term].insert(std::make_pair(doc.first, w));
        }
    }
    // 使用 set 已保证去重并按 (docId, weight) 排序；如需按权重排序可另行复制排序
}

std::vector<int> WeightedInvertedIndex::searchAND(const std::vector<std::string> &terms) const {
    if (terms.empty()) return {};
    // 简化：取每个 term 的 docId 列表进行 AND 交集，不做权重融合
    std::vector<std::vector<int>> lists;
    lists.reserve(terms.size());
    for (const auto &raw : terms) {
        std::string t = raw; // 假设已小写，调用方可预处理
        auto it = postings.find(t);
        if (it == postings.end()) return {};
        std::vector<int> ids;
        ids.reserve(it->second.size());
        for (const auto &p : it->second) ids.push_back(p.first);
        lists.emplace_back(std::move(ids));
    }
    std::sort(lists.begin(), lists.end(), [](const auto &a, const auto &b){ return a.size() < b.size(); });
    std::vector<int> res = lists.front();
    std::vector<int> tmp;
    for (size_t i = 1; i < lists.size(); ++i) {
        tmp.clear();
        const auto &cur = lists[i];
        size_t p = 0, q = 0;
        while (p < res.size() && q < cur.size()) {
            if (res[p] == cur[q]) { tmp.push_back(res[p]); ++p; ++q; }
            else if (res[p] < cur[q]) { ++p; }
            else { ++q; }
        }
        res.swap(tmp);
        if (res.empty()) break;
    }
    return res;
}

std::vector<int> WeightedInvertedIndex::searchANDWeighted(const std::vector<std::string> &terms) const {
    if (terms.empty()) return {};
    // 统计每个 doc 的出现次数和累积权重
    std::unordered_map<int, int> appearCount;
    std::unordered_map<int, double> score;
    const size_t need = terms.size();
    for (const auto &raw : terms) {
        std::string t = raw;
        auto it = postings.find(t);
        if (it == postings.end()) return {};
        for (const auto &p : it->second) {
            appearCount[p.first] += 1;
            score[p.first] += p.second;
        }
    }
    std::vector<std::pair<int, double>> items;
    items.reserve(score.size());
    for (const auto &kv : score) {
        if (static_cast<size_t>(appearCount[kv.first]) == need) {
            items.emplace_back(kv.first, kv.second);
        }
    }
    std::sort(items.begin(), items.end(), [](const auto &a, const auto &b){
        if (a.second != b.second) return a.second > b.second; // weight desc
        return a.first < b.first; // tie-break by docId
    });
    std::vector<int> res;
    res.reserve(items.size());
    for (const auto &e : items) res.push_back(e.first);
    return res;
}

std::vector<int> WeightedInvertedIndex::searchORWeighted(const std::vector<std::string> &terms) const {
    if (terms.empty()) return {};
    std::unordered_map<int, double> score;
    for (const auto &raw : terms) {
        std::string t = raw;
        auto it = postings.find(t);
        if (it == postings.end()) continue;
        for (const auto &p : it->second) score[p.first] += p.second;
    }
    std::vector<std::pair<int, double>> items(score.begin(), score.end());
    std::sort(items.begin(), items.end(), [](const auto &a, const auto &b){
        if (a.second != b.second) return a.second > b.second; // weight desc
        return a.first < b.first;
    });
    std::vector<int> res;
    res.reserve(items.size());
    for (const auto &e : items) res.push_back(e.first);
    return res;
}


