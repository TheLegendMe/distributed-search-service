#include "command_handler.h"
#include "tokenizer.h"
#include "offline_pipeline.h"
#include "keyword_dict.h"
#include "keyword_recommender.h"
#include "weighted_inverted_index.h"
#include "search_engine.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>

using json = nlohmann::json;

// ========== BuildIndexCommand ==========
BuildIndexCommand::BuildIndexCommand(const AppConfig &cfg) : config_(cfg) {}

int BuildIndexCommand::execute() {
    namespace fs = std::filesystem;
    
    // 初始化 Jieba
    if (!config_.jieba_dict_dir.empty()) {
        JiebaTokenizer::instance().initialize(config_.jieba_dict_dir);
    }
    
    // 收集 XML 文件
    std::vector<std::string> xmls;
    if (fs::exists(config_.input_dir) && fs::is_directory(config_.input_dir)) {
        for (const auto &entry : fs::directory_iterator(config_.input_dir)) {
            if (!entry.is_regular_file()) continue;
            const auto &p = entry.path();
            if (p.has_extension() && (p.extension() == ".xml" || p.extension() == ".XML")) {
                xmls.push_back(p.string());
            }
        }
    }
    
    if (xmls.empty()) {
        std::cout << "No XML files found in " << config_.input_dir << "\n";
        return 1;
    }
    
    // 运行离线流水线
    OfflinePipeline pipeline;
    bool ok = pipeline.run(xmls, config_.output_dir, config_.simhash_threshold);
    std::cout << (ok ? "Index build completed successfully\n" : "Index build failed\n");
    return ok ? 0 : 1;
}

// ========== BuildKeywordDictCommand ==========
BuildKeywordDictCommand::BuildKeywordDictCommand(const AppConfig &cfg) : config_(cfg) {}

int BuildKeywordDictCommand::execute() {
    // 初始化 Jieba
    if (!config_.jieba_dict_dir.empty()) {
        JiebaTokenizer::instance().initialize(config_.jieba_dict_dir);
    }
    
    if (config_.candidates_file.empty()) {
        std::cout << "CANDIDATES_FILE not configured\n";
        return 1;
    }
    
    // 构建关键词字典
    KeywordDictBuildResult dict;
    if (!buildKeywordDict(config_.candidates_file, dict)) {
        std::cout << "Failed to build keyword dictionary\n";
        return 1;
    }
    
    // 写入文件
    std::string dict_path, index_path;
    if (!writeKeywordDictFiles(dict, config_.keyword_output_dir, dict_path, index_path)) {
        std::cout << "Failed to write keyword files\n";
        return 1;
    }
    
    std::cout << "Keyword dictionary built successfully:\n";
    std::cout << "  Dictionary: " << dict_path << "\n";
    std::cout << "  Index:      " << index_path << "\n";
    return 0;
}

// ========== QueryCommand ==========
QueryCommand::QueryCommand(const AppConfig &cfg, const std::vector<std::string> &terms, size_t topK)
    : config_(cfg), terms_(terms), topK_(topK) {}

int QueryCommand::execute() {
    if (terms_.empty()) {
        std::cout << "[]\n";
        return 0;
    }
    
    // 构建路径
    namespace fs = std::filesystem;
    std::string index_path = (fs::path(config_.index_dir) / "index.txt").string();
    std::string pages_path = (fs::path(config_.index_dir) / "pages.bin").string();
    std::string offsets_path = (fs::path(config_.index_dir) / "offsets.bin").string();
    
    // 统计文档数
    size_t total_docs = 0;
    {
        std::ifstream fin(offsets_path);
        if (!fin) {
            std::cout << "[]\n";
            return 0;
        }
        int id;
        long long off;
        while (fin >> id >> off) {
            (void)id; (void)off;
            ++total_docs;
        }
    }
    
    // 加载索引
    WeightedInvertedIndex index;
    if (!index.loadFromFile(index_path, total_docs)) {
        std::cout << "[]\n";
        return 0;
    }
    
    // 执行查询
    SearchEngine engine(index, pages_path, offsets_path);
    engine.loadOffsets();
    auto results = engine.queryRanked(terms_, topK_);
    
    // 构建 JSON
    json output = json::array();
    for (const auto &r : results) {
        json item;
        item["docid"] = r.docid;
        item["score"] = r.score;
        item["title"] = r.title;
        item["link"] = r.link;
        item["summary"] = r.summary;
        output.push_back(item);
    }
    
    std::cout << output.dump() << "\n";
    return 0;
}

// ========== RecommendCommand ==========
RecommendCommand::RecommendCommand(const AppConfig &cfg, const std::string &input, size_t topK)
    : config_(cfg), input_(input), topK_(topK) {}

int RecommendCommand::execute() {
    if (input_.empty()) {
        std::cout << "[]\n";
        return 0;
    }
    
    // 确定字典路径
    namespace fs = std::filesystem;
    std::error_code ec;
    std::string dict_path = config_.keyword_dict_dir;
    if (fs::is_directory(dict_path, ec)) {
        dict_path = (fs::path(dict_path) / "keyword_dict.txt").string();
    }
    
    // 加载关键词字典
    std::vector<std::string> words;
    std::vector<uint32_t> freqs;
    if (!loadKeywordDictFile(dict_path, words, freqs)) {
        std::cout << "[]\n";
        return 0;
    }
    
    // 执行推荐
    auto suggestions = recommendKeywords(input_, words, freqs, topK_);
    
    // 构建 JSON
    json output = json::array();
    for (const auto &sug : suggestions) {
        json item;
        item["word"] = sug.word;
        item["distance"] = sug.distance;
        item["frequency"] = sug.frequency;
        output.push_back(item);
    }
    
    std::cout << output.dump() << "\n";
    return 0;
}

