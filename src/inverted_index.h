#pragma once
#include <unordered_map>
#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <algorithm>
#include <cctype>
#include "thread_pool.h"

class InvertedIndex {
public:
    void addDocument(int document_id, const std::string &text);
    void buildParallel(const std::vector<std::pair<int, std::string>> &documents, ThreadPool &pool);
    std::vector<int> searchAND(const std::vector<std::string> &terms) const;
    std::vector<int> searchOR(const std::vector<std::string> &terms) const;

private:
    static void tokenize(const std::string &text, std::vector<std::string> &out_tokens);
    void mergePartial(const std::unordered_map<std::string, std::vector<int>> &partial);

    std::unordered_map<std::string, std::vector<int>> term_to_postings;
    mutable std::mutex index_mutex;
};


