#include "weighted_inverted_index.h"
#include "tokenizer.h"
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

void WeightedInvertedIndex::build(const std::vector<std::pair<int, std::string>> &documents) {
    postings.clear();
    if (documents.empty()) return;
    total_docs = documents.size();

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

std::vector<std::pair<int, double>> WeightedInvertedIndex::searchANDCosineRanked(const std::vector<std::string> &terms) const {
    std::vector<std::pair<int, double>> empty;
    if (terms.empty()) return empty;

    // Step 1: 构建查询向量 X 的 TF-IDF
    std::unordered_map<std::string, int> qtf_raw;
    for (const auto &raw : terms) {
        std::string t = raw;
        qtf_raw[t] += 1;
    }
    int q_max_tf = 0;
    for (const auto &kv : qtf_raw) if (kv.second > q_max_tf) q_max_tf = kv.second;
    if (q_max_tf == 0) return empty;

    const double N = static_cast<double>(total_docs == 0 ? 1 : total_docs);
    std::unordered_map<std::string, double> qvec; // X
    qvec.reserve(qtf_raw.size());
    for (const auto &kv : qtf_raw) {
        const std::string &term = kv.first;
        auto pit = postings.find(term);
        if (pit == postings.end()) return empty; // 有词不在索引中，直接无结果（AND 语义）
        int df_t = static_cast<int>(pit->second.size());
        double tf_norm = 0.5 + 0.5 * (static_cast<double>(kv.second) / static_cast<double>(q_max_tf));
        double idf = std::log((N + 1.0) / (static_cast<double>(df_t) + 1.0)) + 1.0;
        qvec[term] = tf_norm * idf;
    }
    // |X|
    double qnorm2 = 0.0;
    for (const auto &kv : qvec) qnorm2 += kv.second * kv.second;
    double qnorm = qnorm2 > 0.0 ? std::sqrt(qnorm2) : 0.0;
    if (qnorm == 0.0) return empty;

    // Step 2: 求 AND 候选文档集合（docId 交集）
    std::vector<std::vector<int>> lists;
    lists.reserve(terms.size());
    for (const auto &raw : terms) {
        const auto &setref = postings.at(raw);
        std::vector<int> ids;
        ids.reserve(setref.size());
        for (const auto &p : setref) ids.push_back(p.first);
        lists.emplace_back(std::move(ids));
    }
    std::sort(lists.begin(), lists.end(), [](const auto &a, const auto &b){ return a.size() < b.size(); });
    std::vector<int> candidates = lists.front();
    std::vector<int> tmp;
    for (size_t i = 1; i < lists.size(); ++i) {
        tmp.clear();
        const auto &cur = lists[i];
        size_t p = 0, q = 0;
        while (p < candidates.size() && q < cur.size()) {
            if (candidates[p] == cur[q]) { tmp.push_back(candidates[p]); ++p; ++q; }
            else if (candidates[p] < cur[q]) { ++p; }
            else { ++q; }
        }
        candidates.swap(tmp);
        if (candidates.empty()) return empty;
    }

    // Step 3: 对每个候选计算余弦得分
    std::vector<std::pair<int, double>> scored;
    scored.reserve(candidates.size());
    for (int docId : candidates) {
        // 构建 Y 在查询子空间的权重（仅对查询词）
        double dot = 0.0;
        double ynorm2 = 0.0;
        for (const auto &qkv : qvec) {
            const std::string &term = qkv.first;
            // 在线性扫描 set<pair<int,double>> 中查找 docId
            const auto &pset = postings.at(term);
            double weightY = 0.0;
            // 下述线性搜索在 posting 稀疏较小时可接受，必要时可优化为附加映射
            for (const auto &pd : pset) {
                if (pd.first == docId) { weightY = pd.second; break; }
                if (pd.first > docId) break; // 由于按 docId 排序
            }
            if (weightY != 0.0) {
                dot += qkv.second * weightY;
                ynorm2 += weightY * weightY;
            }
        }
        double ynorm = ynorm2 > 0.0 ? std::sqrt(ynorm2) : 0.0;
        double cos = (qnorm > 0.0 && ynorm > 0.0) ? (dot / (qnorm * ynorm)) : 0.0;
        scored.emplace_back(docId, cos);
    }
    std::sort(scored.begin(), scored.end(), [](const auto &a, const auto &b){
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });
    return scored;
}

bool WeightedInvertedIndex::loadFromFile(const std::string &index_path, size_t total_docs_count) {
    postings.clear();
    total_docs = total_docs_count;
    std::ifstream fin(index_path);
    if (!fin) return false;
    std::string line;
    while (std::getline(fin, line)) {
        if (line.empty()) continue;
        auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string term = line.substr(0, tab);
        std::string rest = line.substr(tab + 1);
        std::set<std::pair<int, double>> setv;
        std::stringstream ss(rest);
        std::string token;
        while (std::getline(ss, token, ',')) {
            if (token.empty()) continue;
            auto colon = token.find(':');
            if (colon == std::string::npos) continue;
            int docid = std::atoi(token.substr(0, colon).c_str());
            double w = std::atof(token.substr(colon + 1).c_str());
            setv.insert({docid, w});
        }
        if (!setv.empty()) postings[term] = std::move(setv);
    }
    return !postings.empty();
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


