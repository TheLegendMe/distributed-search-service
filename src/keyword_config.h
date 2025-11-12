#pragma once
#include <string>

// 简单配置：key=value，每行一项，支持的 key：
// CANDIDATES_FILE: 候选词文件路径
// OUTPUT_DIR: 输出目录
struct KeywordConfig {
    std::string candidates_file;
    std::string output_dir;
    std::string jieba_dict_dir;
};

bool loadKeywordConfig(const std::string &path, KeywordConfig &cfg);


