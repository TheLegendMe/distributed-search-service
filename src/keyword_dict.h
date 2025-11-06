#pragma once
#include <string>
#include <vector>
#include <unordered_map>

// 离线关键词字典：
// - 输入：候选词文件（每行一个词），自动转小写
// - 统计频次，输出 dict 文件："word frequency" 每行一条
// - 生成索引：字符 -> 词ID集合（以逗号分隔），用于缩小候选集

struct KeywordDictBuildResult {
    std::vector<std::string> words;            // id -> word
    std::vector<uint32_t> frequencies;         // id -> freq
};

bool buildKeywordDict(const std::string &candidates_file,
                      KeywordDictBuildResult &out);

bool writeKeywordDictFiles(const KeywordDictBuildResult &data,
                           const std::string &output_dir,
                           std::string &dict_path_out,
                           std::string &index_path_out);


