#include "inverted_index.h"

static inline bool isAlnum(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0;
}

void InvertedIndex::tokenize(const std::string &text, std::vector<std::string> &out_tokens) {
    out_tokens.clear();
    std::string token;
    token.reserve(32);
    for (char ch : text) {
        if (isAlnum(ch)) {
            token.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        } else if (!token.empty()) {
            out_tokens.push_back(token);
            token.clear();
        }
    }
    if (!token.empty()) out_tokens.push_back(token);
}

void InvertedIndex::mergePartial(const std::unordered_map<std::string, std::vector<int>> &partial) {
    std::lock_guard<std::mutex> lock(index_mutex);
    for (const auto &kv : partial) {
        auto &dest = term_to_postings[kv.first];
        dest.insert(dest.end(), kv.second.begin(), kv.second.end());
    }
}

void InvertedIndex::addDocument(int document_id, const std::string &text) {
    std::vector<std::string> tokens;
    tokenize(text, tokens);

    std::unordered_map<std::string, std::vector<int>> local;
    local.reserve(tokens.size());
    for (const auto &t : tokens) {
        auto &v = local[t];
        if (v.empty() || v.back() != document_id) v.push_back(document_id);
    }
    mergePartial(local);
}

void InvertedIndex::buildParallel(const std::vector<std::pair<int, std::string>> &documents, ThreadPool &pool) {
    const size_t n = documents.size();
    if (n == 0) return;

    const size_t workers = std::max<size_t>(1, std::thread::hardware_concurrency());
    const size_t chunk = std::max<size_t>(1, (n + workers - 1) / workers);

    std::atomic<size_t> next_index{0};
    std::atomic<size_t> remaining{workers};
    std::mutex done_mtx;
    std::condition_variable done_cv;

    for (size_t i = 0; i < workers; ++i) {
        pool.enqueue([&, i]() {
            for (;;) {
                size_t start = next_index.fetch_add(chunk);
                if (start >= n) break;
                size_t end = std::min(n, start + chunk);

                std::unordered_map<std::string, std::vector<int>> partial;
                partial.reserve(256);

                for (size_t j = start; j < end; ++j) {
                    int doc_id = documents[j].first;
                    const std::string &text = documents[j].second;
                    std::vector<std::string> tokens;
                    tokenize(text, tokens);
                    for (const auto &tok : tokens) {
                        auto &vec = partial[tok];
                        if (vec.empty() || vec.back() != doc_id) vec.push_back(doc_id);
                    }
                }

                mergePartial(partial);
            }
            if (remaining.fetch_sub(1) == 1) {
                std::lock_guard<std::mutex> lk(done_mtx);
                done_cv.notify_one();
            }
        });
    }

    std::unique_lock<std::mutex> lk(done_mtx);
    done_cv.wait(lk, [&]() { return remaining.load() == 0; });

    for (auto &kv : term_to_postings) {
        auto &v = kv.second;
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
    }
}

std::vector<int> InvertedIndex::searchAND(const std::vector<std::string> &terms) const {
    if (terms.empty()) return {};
    std::vector<std::vector<int>> postings_lists;
    postings_lists.reserve(terms.size());
    for (const auto &t_raw : terms) {
        std::string t;
        t.reserve(t_raw.size());
        for (char c : t_raw) t.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        auto it = term_to_postings.find(t);
        if (it == term_to_postings.end()) return {};
        postings_lists.push_back(it->second);
    }
    std::sort(postings_lists.begin(), postings_lists.end(), [](const auto &a, const auto &b){ return a.size() < b.size(); });
    std::vector<int> result = postings_lists.front();
    std::vector<int> temp;
    for (size_t i = 1; i < postings_lists.size(); ++i) {
        temp.clear();
        const auto &cur = postings_lists[i];
        size_t p = 0, q = 0;
        while (p < result.size() && q < cur.size()) {
            if (result[p] == cur[q]) { temp.push_back(result[p]); ++p; ++q; }
            else if (result[p] < cur[q]) { ++p; }
            else { ++q; }
        }
        result.swap(temp);
        if (result.empty()) break;
    }
    return result;
}

std::vector<int> InvertedIndex::searchOR(const std::vector<std::string> &terms) const {
    if (terms.empty()) return {};
    std::vector<int> result;
    for (const auto &t_raw : terms) {
        std::string t;
        t.reserve(t_raw.size());
        for (char c : t_raw) t.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        auto it = term_to_postings.find(t);
        if (it == term_to_postings.end()) continue;
        const auto &v = it->second;
        result.insert(result.end(), v.begin(), v.end());
    }
    if (result.empty()) return result;
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}


