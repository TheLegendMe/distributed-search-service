#include "keyword_dict.h"
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <sys/stat.h>
#include <sys/types.h>
#include <filesystem>
#include <sstream>
#include <vector>
#include <cctype>

#include "tokenizer.h"

static inline std::string toLowerAscii(const std::string &s) {
    std::string out; out.reserve(s.size());
    for (unsigned char c : s) out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}

static inline void trimAscii(std::string &s) {
    auto notspace = [](unsigned char ch){ return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
}

static inline void trimAsciiPunct(std::string &s) {
    auto ispunct = [](unsigned char ch){ return std::ispunct(ch); };
    while (!s.empty() && ispunct(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && ispunct(static_cast<unsigned char>(s.back()))) s.pop_back();
}

static inline bool isMeaningfulToken(const std::string &token) {
    if (token.empty()) return false;
    if (token.size() < 3) return false;  // 中文字符至少3字节
    
    bool has_chinese = false;
    size_t i = 0;
    
    // 遍历 UTF-8 字符
    while (i < token.size()) {
        unsigned char c0 = static_cast<unsigned char>(token[i]);
        
        // ASCII 字符（跳过）
        if (c0 < 0x80) {
            i++;
            continue;
        }
        
        // 3字节 UTF-8（包含大部分中文字符）
        if ((c0 & 0xF0) == 0xE0 && i + 2 < token.size()) {
            unsigned char c1 = static_cast<unsigned char>(token[i + 1]);
            unsigned char c2 = static_cast<unsigned char>(token[i + 2]);
            
            // 中文字符范围：U+4E00 到 U+9FFF (CJK Unified Ideographs)
            // UTF-8 编码: E4 B8 80 到 E9 BF BF
            if (c0 >= 0xE4 && c0 <= 0xE9) {
                if ((c0 == 0xE4 && c1 >= 0xB8) || 
                    (c0 >= 0xE5 && c0 <= 0xE8) ||
                    (c0 == 0xE9 && c1 <= 0xBF)) {
                    has_chinese = true;
                }
            }
            i += 3;
        }
        // 2字节 UTF-8（跳过，一般不是中文）
        else if ((c0 & 0xE0) == 0xC0 && i + 1 < token.size()) {
            i += 2;
        }
        // 4字节 UTF-8（跳过，一般不是中文）
        else if ((c0 & 0xF8) == 0xF0 && i + 3 < token.size()) {
            i += 4;
        }
        else {
            i++;
        }
    }
    
    return has_chinese;
}

static bool ensureDir(const std::string &dir) {
    struct stat st{};
    if (stat(dir.c_str(), &st) == 0) return S_ISDIR(st.st_mode);
    return ::mkdir(dir.c_str(), 0755) == 0;
}

static bool collectFromCandidateFile(const std::string &path,
                                     std::unordered_map<std::string, uint32_t> &freq) {
    std::ifstream fin(path);
    if (!fin) return false;
    std::string line;
    while (std::getline(fin, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        trimAscii(line);
        trimAsciiPunct(line);
        if (line.empty()) continue;
        auto lowered = toLowerAscii(line);
        if (!isMeaningfulToken(lowered)) continue;
        ++freq[lowered];
    }
    return !freq.empty();
}

static bool collectFromDirectory(const std::string &dir,
                                 std::unordered_map<std::string, uint32_t> &freq) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return false;

    JiebaTokenizer &tokenizer = JiebaTokenizer::instance();
    std::vector<std::string> tokens;

    fs::recursive_directory_iterator it(dir, ec), end;
    if (ec) return false;
    for (; it != end; ++it) {
        if (it->is_directory()) continue;
        if (!it->is_regular_file()) continue;

        std::ifstream fin(it->path());
        if (!fin) continue;
        std::ostringstream oss;
        oss << fin.rdbuf();
        std::string content = oss.str();
        if (content.empty()) continue;

        tokenizer.tokenize(content, tokens);
        for (auto &tok : tokens) {
            std::string cleaned = tok;
            trimAscii(cleaned);
            trimAsciiPunct(cleaned);
            if (!isMeaningfulToken(cleaned)) continue;
            ++freq[cleaned];
        }
    }
    return !freq.empty();
}

bool buildKeywordDict(const std::string &candidates_file,
                      KeywordDictBuildResult &out) {
    std::unordered_map<std::string, uint32_t> freq;
    namespace fs = std::filesystem;
    bool ok = false;
    std::error_code ec;
    auto status = fs::status(candidates_file, ec);
    if (!candidates_file.empty() && !ec) {
        if (fs::is_directory(status)) {
            ok = collectFromDirectory(candidates_file, freq);
        } else if (fs::is_regular_file(status)) {
            ok = collectFromCandidateFile(candidates_file, freq);
        }
    }
    if (!ok) {
        // 尝试按文件读取（兼容旧路径或 status 失败场景）
        ok = collectFromCandidateFile(candidates_file, freq);
    }
    if (!ok || freq.empty()) return false;
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

    // 写索引：每个词的首字(UTF-8完整字符)\t id1,id2,... （id 是 0-based）
    // 按词的首字符建立索引，支持中文等多字节字符
    {
        std::unordered_map<std::string, std::vector<size_t>> idxmap;
        for (size_t i = 0; i < data.words.size(); ++i) {
            const std::string &w = data.words[i];
            if (w.empty()) continue;
            // 提取首个UTF-8字符
            std::string first_char;
            unsigned char c0 = static_cast<unsigned char>(w[0]);
            if (c0 < 0x80) {
                // ASCII 单字节
                first_char = w.substr(0, 1);
            } else if ((c0 & 0xE0) == 0xC0 && w.size() >= 2) {
                // 2字节 UTF-8
                first_char = w.substr(0, 2);
            } else if ((c0 & 0xF0) == 0xE0 && w.size() >= 3) {
                // 3字节 UTF-8 (中文常见)
                first_char = w.substr(0, 3);
            } else if ((c0 & 0xF8) == 0xF0 && w.size() >= 4) {
                // 4字节 UTF-8
                first_char = w.substr(0, 4);
            } else {
                // 无法识别，取第一个字节
                first_char = w.substr(0, 1);
            }
            idxmap[first_char].push_back(i);
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


