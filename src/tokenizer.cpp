#include "tokenizer.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>

// 尽量将第三方库包含限制在 .cpp 中
#include <cppjieba/Jieba.hpp>

struct JiebaTokenizer::Impl {
    cppjieba::Jieba *jieba;
    Impl(): jieba(nullptr) {}
};

static std::string envOrDefault(const char *name, const std::string &fallback) {
    const char *v = std::getenv(name);
    return v ? std::string(v) : fallback;
}

JiebaTokenizer::JiebaTokenizer()
    : initialized(false), impl(new Impl) {}

JiebaTokenizer::~JiebaTokenizer() {
    if (impl && impl->jieba) {
        delete impl->jieba;
        impl->jieba = nullptr;
    }
    delete impl;
}

JiebaTokenizer &JiebaTokenizer::instance() {
    static JiebaTokenizer inst;
    return inst;
}

void JiebaTokenizer::initialize(const std::string &dict_dir) {
    std::lock_guard<std::mutex> lk(init_mutex);
    if (initialized) return;
    dict_path = dict_dir + "/jieba.dict.utf8";
    hmm_path = dict_dir + "/hmm_model.utf8";
    user_dict_path = dict_dir + "/user.dict.utf8";
    idf_path = dict_dir + "/idf.utf8";
    stop_words_path = dict_dir + "/stop_words.utf8";
    impl->jieba = new cppjieba::Jieba(dict_path, hmm_path, user_dict_path, idf_path, stop_words_path);
    initialized = true;
}

void JiebaTokenizer::ensureInitialized() {
    if (initialized) return;
    std::lock_guard<std::mutex> lk(init_mutex);
    if (initialized) return;
    // 默认从环境变量 JIEBA_DICT_DIR 读取词典目录
    std::string dir = envOrDefault("JIEBA_DICT_DIR", "./dict");
    dict_path = dir + "/jieba.dict.utf8";
    hmm_path = dir + "/hmm_model.utf8";
    user_dict_path = dir + "/user.dict.utf8";
    idf_path = dir + "/idf.utf8";
    stop_words_path = dir + "/stop_words.utf8";
    impl->jieba = new cppjieba::Jieba(dict_path, hmm_path, user_dict_path, idf_path, stop_words_path);
    initialized = true;
}

void JiebaTokenizer::tokenize(const std::string &text, std::vector<std::string> &out_tokens) {
    ensureInitialized();
    std::vector<std::string> words;
    impl->jieba->CutForSearch(text, words);
    out_tokens.clear();
    out_tokens.reserve(words.size());
    for (auto &w : words) {
        std::string t;
        t.reserve(w.size());
        for (unsigned char c : w) {
            // 英文转小写；中文保持不变
            t.push_back(std::isalpha(c) ? static_cast<char>(std::tolower(c)) : static_cast<char>(c));
        }
        out_tokens.push_back(std::move(t));
    }
}


