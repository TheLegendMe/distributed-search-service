#include "keyword_dict.h"
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <sys/stat.h>
#include <sys/types.h>

static inline std::string toLowerAscii(const std::string &s) {
    std::string out; out.reserve(s.size());
    for (unsigned char c : s) out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}

static bool ensureDir(const std::string &dir) {
    struct stat st{};
    if (stat(dir.c_str(), &st) == 0) return S_ISDIR(st.st_mode);
    return ::mkdir(dir.c_str(), 0755) == 0;
}

bool buildKeywordDict(const std::string &candidates_file,
                      KeywordDictBuildResult &out) {
    std::ifstream fin(candidates_file);
    if (!fin) return false;
    std::unordered_map<std::string, uint32_t> freq;
    std::string line;
    while (std::getline(fin, line)) {
        // trim CR/LF
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line.empty()) continue;
        auto lower = toLowerAscii(line);
        ++freq[lower];
    }
    if (freq.empty()) return false;
    out.words.clear();
    out.frequencies.clear();
    out.words.reserve(freq.size());
    out.frequencies.reserve(freq.size());
    for (const auto &kv : freq) {
        out.words.push_back(kv.first);
        out.frequencies.push_back(kv.second);
    }
    // 保持稳定顺序：按词典序排序，频次大的可以在搜索时再处理
    std::vector<size_t> idx(out.words.size());
    for (size_t i = 0; i < idx.size(); ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b){ return out.words[a] < out.words[b]; });
    std::vector<std::string> words2; words2.reserve(idx.size());
    std::vector<uint32_t> freq2; freq2.reserve(idx.size());
    for (size_t i : idx) { words2.push_back(std::move(out.words[i])); freq2.push_back(out.frequencies[i]); }
    out.words.swap(words2);
    out.frequencies.swap(freq2);
    return true;
}

bool writeKeywordDictFiles(const KeywordDictBuildResult &data,
                           const std::string &output_dir,
                           std::string &dict_path_out,
                           std::string &index_path_out) {
    if (!ensureDir(output_dir)) return false;
    dict_path_out = output_dir + "/keyword_dict.txt";
    index_path_out = output_dir + "/keyword_index.txt";

    // 写字典：word frequency
    {
        std::ofstream dout(dict_path_out);
        if (!dout) return false;
        for (size_t i = 0; i < data.words.size(); ++i) {
            dout << data.words[i] << ' ' << data.frequencies[i] << '\n';
        }
    }

    // 写索引：char\t id1,id2,... （id 是 0-based）
    // 为避免重复，将词中字符去重后加入索引
    {
        std::unordered_map<char, std::vector<size_t>> idxmap;
        for (size_t i = 0; i < data.words.size(); ++i) {
            const std::string &w = data.words[i];
            bool seen[256] = {false};
            for (unsigned char c : w) {
                if (!seen[c]) { idxmap[static_cast<char>(c)].push_back(i); seen[c] = true; }
            }
        }
        std::ofstream iout(index_path_out);
        if (!iout) return false;
        for (auto &kv : idxmap) {
            iout << kv.first << '\t';
            auto &vec = kv.second;
            std::sort(vec.begin(), vec.end());
            for (size_t j = 0; j < vec.size(); ++j) {
                iout << vec[j];
                if (j + 1 < vec.size()) iout << ',';
            }
            iout << '\n';
        }
    }
    return true;
}


