#include "dynamic_index.h"
#include "tokenizer.h"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>

bool DynamicInvertedIndex::loadFromFile(const std::string &index_path, size_t total_docs_count) {
    std::unique_lock lock(mutex_);
    
    std::ifstream ifs(index_path);
    if (!ifs) return false;
    
    postings_.clear();
    total_docs_ = total_docs_count;
    
    std::string line;
    while (std::getline(ifs, line)) {
        std::istringstream iss(line);
        std::string term;
        iss >> term;
        
        int docid;
        double weight;
        while (iss >> docid >> weight) {
            postings_[term].insert({docid, weight});
        }
    }
    
    return true;
}

void DynamicInvertedIndex::addDocument(int docid, const std::string &text) {
    std::unique_lock lock(mutex_);
    
    // 如果文档已存在，先删除
    if (doc_tokens_.count(docid)) {
        deleted_docs_.insert(docid);
    }
    
    // 分词
    auto tokens = tokenize(text);
    doc_tokens_[docid] = tokens;
    deleted_docs_.erase(docid);
    
    // 计算TF-IDF并更新索引
    computeTFIDF(docid, tokens);
    
    total_docs_++;
    
    // 重新计算IDF
    recomputeIDF();
}

void DynamicInvertedIndex::addDocument(int docid, const DocumentMeta &meta) {
    std::unique_lock lock(mutex_);
    
    // 如果文档已存在，先删除
    if (doc_tokens_.count(docid)) {
        deleted_docs_.insert(docid);
    }
    
    // 存储元数据
    doc_metadata_[docid] = meta;
    
    // 分词（使用完整文本）
    auto tokens = tokenize(meta.text);
    doc_tokens_[docid] = tokens;
    deleted_docs_.erase(docid);
    
    // 计算TF-IDF并更新索引
    computeTFIDF(docid, tokens);
    
    total_docs_++;
    
    // 重新计算IDF
    recomputeIDF();
}

bool DynamicInvertedIndex::getDocumentMeta(int docid, DocumentMeta &meta) const {
    std::shared_lock lock(mutex_);
    
    auto it = doc_metadata_.find(docid);
    if (it == doc_metadata_.end() || deleted_docs_.count(docid)) {
        return false;
    }
    
    meta = it->second;
    return true;
}

void DynamicInvertedIndex::addDocuments(const std::vector<std::pair<int, std::string>> &documents) {
    std::unique_lock lock(mutex_);
    
    for (const auto &[docid, text] : documents) {
        auto tokens = tokenize(text);
        doc_tokens_[docid] = tokens;
        deleted_docs_.erase(docid);
        computeTFIDF(docid, tokens);
    }
    
    total_docs_ += documents.size();
    recomputeIDF();
}

void DynamicInvertedIndex::removeDocument(int docid) {
    std::unique_lock lock(mutex_);
    
    deleted_docs_.insert(docid);
    
    // 自动压缩检查
    if (needsCompaction()) {
        compact();
    }
}

void DynamicInvertedIndex::updateDocument(int docid, const std::string &new_text) {
    removeDocument(docid);
    addDocument(docid, new_text);
}

std::vector<std::pair<int, double>> DynamicInvertedIndex::searchANDCosineRanked(
    const std::vector<std::string> &terms) const {
    
    std::shared_lock lock(mutex_);
    
    if (terms.empty()) return {};
    
    // 1. 找到包含所有查询词的文档（AND语义）
    std::unordered_map<int, std::vector<double>> doc_weights;
    
    for (size_t i = 0; i < terms.size(); ++i) {
        auto it = postings_.find(terms[i]);
        if (it == postings_.end()) return {};  // 有词不存在，返回空
        
        for (const auto &[docid, weight] : it->second) {
            // 跳过已删除的文档
            if (deleted_docs_.count(docid)) continue;
            
            if (doc_weights[docid].empty()) {
                doc_weights[docid].resize(terms.size(), 0.0);
            }
            doc_weights[docid][i] = weight;
        }
    }
    
    // 2. 过滤：只保留包含所有查询词的文档
    std::vector<int> valid_docs;
    for (const auto &[docid, weights] : doc_weights) {
        bool has_all = true;
        for (double w : weights) {
            if (w == 0.0) {
                has_all = false;
                break;
            }
        }
        if (has_all) {
            valid_docs.push_back(docid);
        }
    }
    
    // 3. 计算查询向量的权重
    std::vector<double> query_weights(terms.size());
    for (size_t i = 0; i < terms.size(); ++i) {
        double tf = 1.0;  // 查询词TF=1
        size_t df = postings_.count(terms[i]) ? postings_.at(terms[i]).size() : 1;
        double idf = std::log((double)total_docs_ / df);
        query_weights[i] = tf * idf;
    }
    
    // 4. 计算余弦相似度
    std::vector<std::pair<int, double>> results;
    for (int docid : valid_docs) {
        const auto &doc_vec = doc_weights[docid];
        
        double dot_product = 0.0, doc_norm = 0.0, query_norm = 0.0;
        for (size_t i = 0; i < terms.size(); ++i) {
            dot_product += query_weights[i] * doc_vec[i];
            doc_norm += doc_vec[i] * doc_vec[i];
            query_norm += query_weights[i] * query_weights[i];
        }
        
        double cosine = dot_product / (std::sqrt(doc_norm) * std::sqrt(query_norm));
        results.push_back({docid, cosine});
    }
    
    // 5. 按相似度降序排序
    std::sort(results.begin(), results.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });
    
    return results;
}

DynamicInvertedIndex::Stats DynamicInvertedIndex::getStats() const {
    std::shared_lock lock(mutex_);
    
    return {
        total_docs_,
        total_docs_ - deleted_docs_.size(),
        deleted_docs_.size(),
        postings_.size(),
        0  // pending_updates
    };
}

bool DynamicInvertedIndex::needsCompaction() const {
    std::shared_lock lock(mutex_);
    return deleted_docs_.size() > total_docs_ * 0.2;  // 删除超过20%
}

bool DynamicInvertedIndex::saveToFile(const std::string &index_path) const {
    std::shared_lock lock(mutex_);
    
    std::ofstream ofs(index_path);
    if (!ofs) return false;
    
    for (const auto &[term, postings] : postings_) {
        ofs << term;
        for (const auto &[docid, weight] : postings) {
            if (deleted_docs_.count(docid)) continue;  // 跳过已删除
            ofs << " " << docid << " " << weight;
        }
        ofs << "\n";
    }
    
    return true;
}

void DynamicInvertedIndex::compact() {
    // 已在锁内调用，不需要再加锁
    
    // 从索引中移除已删除的文档
    for (auto &[term, postings] : postings_) {
        for (auto it = postings.begin(); it != postings.end(); ) {
            if (deleted_docs_.count(it->first)) {
                it = postings.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // 清空删除列表
    total_docs_ -= deleted_docs_.size();
    deleted_docs_.clear();
    
    // 重新计算IDF
    recomputeIDF();
}

std::vector<std::string> DynamicInvertedIndex::tokenize(const std::string &text) const {
    // 使用JiebaTokenizer分词
    std::vector<std::string> tokens;
    JiebaTokenizer::instance().tokenize(text, tokens);
    return tokens;
}

void DynamicInvertedIndex::computeTFIDF(int docid, const std::vector<std::string> &tokens) {
    // 计算词频
    std::unordered_map<std::string, int> tf_map;
    for (const auto &token : tokens) {
        tf_map[token]++;
    }
    
    // 计算TF-IDF并更新倒排表
    for (const auto &[term, tf] : tf_map) {
        double tf_value = (double)tf / tokens.size();
        
        // DF会在recomputeIDF中统一计算
        postings_[term].insert({docid, tf_value});
    }
}

void DynamicInvertedIndex::recomputeIDF() {
    // 重新计算所有词的IDF并更新权重
    for (auto &[term, postings] : postings_) {
        size_t df = postings.size();
        double idf = std::log((double)total_docs_ / df);
        
        // 更新所有文档的TF-IDF权重
        std::set<std::pair<int, double>> new_postings;
        for (const auto &[docid, tf] : postings) {
            if (!deleted_docs_.count(docid)) {
                new_postings.insert({docid, tf * idf});
            }
        }
        postings = new_postings;
    }
}

