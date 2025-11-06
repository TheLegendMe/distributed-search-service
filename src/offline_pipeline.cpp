#include "offline_pipeline.h"
#include "tokenizer.h"
#include "simhash.h"
#include "weighted_inverted_index.h"
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>
#include <sys/types.h>
#include <iostream>
#include <cctype>

static bool ensureDir(const std::string &dir) {
    struct stat st{};
    if (stat(dir.c_str(), &st) == 0) return S_ISDIR(st.st_mode);
    return ::mkdir(dir.c_str(), 0755) == 0;
}

bool OfflinePipeline::run(const std::vector<std::string> &xml_files, const std::string &output_dir,
                          int simhash_threshold) {
    std::cout << "Running OfflinePipeline with " << xml_files.size() << " XML files" << std::endl;
    if (xml_files.empty()) return false;
    if (!ensureDir(output_dir)) return false;

    // 1) 解析所有 XML，构建网页库
    std::vector<Page> pages;
    for (const auto &f : xml_files) {
        std::cout << "Parsing XML file: " << f << std::endl;
        std::vector<Page> one;
        if (!PageParser::parseFromXmlFile(f, one)) {
            std::cout << "Failed to parse XML file: " << f << std::endl;
            continue;
        }
        pages.insert(pages.end(), one.begin(), one.end());
    }
    if (pages.empty()) return false;

    // 2) 去重：按 SimHash 阈值去重
    std::vector<Page> dedup_pages;
    dedup_pages.reserve(pages.size());
    std::vector<uint64_t> signatures; // 已保留的 simhash
    for (const auto &p : pages) {
        std::vector<std::string> toks;
        JiebaTokenizer::instance().tokenize(p.title + "\n" + p.description, toks);
        uint64_t sig = SimHasher::simhash64(toks);
        bool dup = false;
        for (uint64_t ex : signatures) {
            if (SimHasher::hammingDistance(sig, ex) <= simhash_threshold) { dup = true; break; }
        }
        if (!dup) {
            dedup_pages.push_back(p);
            signatures.push_back(sig);
        }
    }
    if (dedup_pages.empty()) return false;

    // 3) 建立 TF-IDF 倒排索引
    std::vector<std::pair<int, std::string>> docs;
    docs.reserve(dedup_pages.size());
    for (const auto &p : dedup_pages) {
        // 将标题和正文合并作为索引文本
        docs.emplace_back(p.docid, p.title + "\n" + p.description);
    }
    WeightedInvertedIndex index;
    index.build(docs);

    // 4) 生成网页库与偏移库
    const std::string pages_path = output_dir + "/pages.bin";
    const std::string offsets_path = output_dir + "/offsets.bin";
    const std::string index_path = output_dir + "/index.txt";

    std::ofstream pages_out(pages_path, std::ios::out | std::ios::binary);
    std::ofstream offsets_out(offsets_path, std::ios::out | std::ios::binary);
    if (!pages_out || !offsets_out) return false;

    auto sanitize = [](const std::string &in) -> std::string {
        std::string out;
        out.reserve(in.size());
        for (unsigned char c : in) {
            if (c == '\\t' || c == '\\n' || c == '\\r') {
                out.push_back(' ');
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
        // 可选：压缩多余空格
        std::string compact;
        compact.reserve(out.size());
        bool prev_space = false;
        for (char ch : out) {
            bool is_space = (ch == ' ');
            if (is_space && prev_space) continue;
            compact.push_back(ch);
            prev_space = is_space;
        }
        return compact;
    };

    auto xmlEscape = [](const std::string &in) -> std::string {
        std::string out;
        out.reserve(in.size());
        for (char ch : in) {
            switch (ch) {
                case '&': out += "&amp;"; break;
                case '<': out += "&lt;"; break;
                case '>': out += "&gt;"; break;
                case '"': out += "&quot;"; break;
                case '\'': out += "&apos;"; break;
                default: out.push_back(ch); break;
            }
        }
        return out;
    };

    std::streampos offset = 0;
    for (const auto &p : dedup_pages) {
        offsets_out << p.docid << '\t' << offset << '\n';
        std::ostringstream line;
        const std::string link = xmlEscape(sanitize(p.link));
        const std::string title = xmlEscape(sanitize(p.title));
        const std::string desc = xmlEscape(sanitize(p.description));

        // 以易行式（doc/docid/title/link/description）保存
        line << "<doc>\n";
        line << "<docid>" << p.docid << "</docid>\n";
        line << "<title>" << title << "</title>\n";
        line << "<link>" << link << "</link>\n";
        line << "<description>" << desc << "</description>\n";
        line << "</doc>\n";
        const std::string s = line.str();
        pages_out.write(s.data(), static_cast<std::streamsize>(s.size()));
        offset += static_cast<std::streamoff>(s.size());
    }

    // 5) 将倒排索引写出（文本格式，便于调试）
    // term TAB docId:weight,docId:weight,...\n
    std::ofstream index_out(index_path);
    if (!index_out) return false;
    const auto &tbl = index.data();
    for (const auto &kv : tbl) {
        index_out << kv.first << '\t';
        const auto &s = kv.second; // set<pair<int,double>>
        size_t i = 0;
        for (auto it = s.begin(); it != s.end(); ++it, ++i) {
            index_out << it->first << ':' << it->second;
            if (std::next(it) != s.end()) index_out << ',';
        }
        index_out << '\n';
    }

    return true;
}


