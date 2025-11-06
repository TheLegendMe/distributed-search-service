#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <filesystem>
#include "thread_pool.h"
#include "offline_pipeline.h"
#include "tokenizer.h"
#include "weighted_inverted_index.h"
#include "search_engine.h"
#include "keyword_config.h"
#include "keyword_dict.h"

int main(int argc, char **argv) {
    // 离线关键词： --build-keywords <config>
    if (argc == 3 && std::string(argv[1]) == "--build-keywords") {
        KeywordConfig cfg;
        if (!loadKeywordConfig(argv[2], cfg)) {
            std::cout << "failed to load config\n";
            return 1;
        }
        KeywordDictBuildResult dict;
        if (!buildKeywordDict(cfg.candidates_file, dict)) {
            std::cout << "failed to build keyword dict\n";
            return 1;
        }
        std::string dict_path, index_path;
        if (!writeKeywordDictFiles(dict, cfg.output_dir, dict_path, index_path)) {
            std::cout << "failed to write keyword files\n";
            return 1;
        }
        std::cout << "dict: " << dict_path << "\nindex: " << index_path << "\n";
        return 0;
    }
    // 查询模式： --query kw1 kw2 ... [topK]
    if (argc >= 2 && std::string(argv[1]) == "--query") {
        std::vector<std::string> terms;
        size_t topK = 20;
        for (int i = 2; i < argc; ++i) terms.push_back(argv[i]);
        if (!terms.empty()) {
            // 若最后一个是纯数字，当作 topK
            try {
                size_t pos = 0; 
                int maybe = std::stoi(terms.back(), &pos);
                if (pos == terms.back().size() && maybe > 0) { topK = static_cast<size_t>(maybe); terms.pop_back(); }
            } catch (...) {}
        }
        if (terms.empty()) {
            std::cout << "[]\n";
            return 0;
        }

        // 加载索引与页面库
        const std::string out_dir = "./output";
        const std::string index_path = out_dir + "/index.txt";
        const std::string pages_path = out_dir + "/pages.bin";
        const std::string offsets_path = out_dir + "/offsets.bin";

        // 统计文档数（行数）
        size_t total_docs = 0; {
            std::ifstream fin(offsets_path);
            int id; long long off; while (fin >> id >> off) { (void)id; (void)off; ++total_docs; }
        }
        WeightedInvertedIndex index;
        if (!index.loadFromFile(index_path, total_docs)) { std::cout << "[]\n"; return 0; }

        SearchEngine engine(index, pages_path, offsets_path);
        engine.loadOffsets();
        auto results = engine.queryRanked(terms, topK);

        // 输出 JSON 数组
        std::cout << "[";
        for (size_t i = 0; i < results.size(); ++i) {
            const auto &r = results[i];
            // 简单转义
            auto esc = [](const std::string &s){ std::string o; o.reserve(s.size()); for (char ch: s){ switch(ch){ case '"': o += "\\\""; break; case '\\': o += "\\\\"; break; case '\n': o += "\\n"; break; case '\r': o += "\\r"; break; case '\t': o += "\\t"; break; default: o.push_back(ch);} } return o; };
            std::cout << "{\"docid\":" << r.docid
                      << ",\"score\":" << r.score
                      << ",\"title\":\"" << esc(r.title) << "\""
                      << ",\"link\":\"" << esc(r.link) << "\""
                      << ",\"summary\":\"" << esc(r.summary) << "\"}";
            if (i + 1 < results.size()) std::cout << ",";
        }
        std::cout << "]\n";
        return 0;
    }

    // 离线模式：解析 ./input 目录下的 XML 文件，输出到 ./output
    std::string input_dir = "./input";
    std::string out_dir = "./output";

    std::vector<std::string> xmls;
    namespace fs = std::filesystem;
    if (fs::exists(input_dir) && fs::is_directory(input_dir)) {
        for (const auto &entry : fs::directory_iterator(input_dir)) {
            if (!entry.is_regular_file()) continue;
            const auto &p = entry.path();
            if (p.has_extension() && (p.extension() == ".xml" || p.extension() == ".XML")) {
                xmls.push_back(p.string());
            }
        }
    }

    if (xmls.empty()) {
        std::cout << "No XML files found under ./input. Please put XML files there.\n";
        return 1;
    }

    // 显式初始化 Jieba 词典路径，避免默认 ./dict 导致找不到字典
    JiebaTokenizer::instance().initialize("/home/oym/cppjieba/cppjieba-5.0.3/dict");

    OfflinePipeline pipeline;
    bool ok = pipeline.run(xmls, out_dir, /*simhash_threshold*/3);
    std::cout << (ok ? "offline pipeline done\n" : "offline pipeline failed\n");
}
