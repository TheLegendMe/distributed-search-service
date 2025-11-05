#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include "thread_pool.h"
#include "offline_pipeline.h"

int main(int argc, char **argv) {
    // 用法：程序 xml1.xml xml2.xml ... [output_dir]
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <xml_file...> [output_dir]\n";
        return 0;
    }
    std::vector<std::string> xmls;
    for (int i = 1; i < argc; ++i) xmls.push_back(argv[i]);
    std::string out_dir = "./output";
    if (xmls.size() >= 2) { out_dir = xmls.back(); xmls.pop_back(); }

    // 可选：初始化 Jieba 词典目录（也可通过环境变量 JIEBA_DICT_DIR 设置）
    // JiebaTokenizer::instance().initialize("/path/to/cppjieba/dict");

    OfflinePipeline pipeline;
    bool ok = pipeline.run(xmls, out_dir, /*simhash_threshold*/3);
    std::cout << (ok ? "offline pipeline done\n" : "offline pipeline failed\n");
}
