#include "search_engine.h"
#include <algorithm>
#include <sstream>

SearchEngine::SearchEngine(const WeightedInvertedIndex &idx,
                           const std::string &pages,
                           const std::string &offsets)
    : index(idx), pages_path(pages), offsets_path(offsets) {}

bool SearchEngine::loadOffsets() {
    std::ifstream fin(offsets_path);
    if (!fin) return false;
    docid_to_offset.clear();
    int id; long long off;
    while (fin >> id >> off) {
        docid_to_offset[id] = static_cast<std::streampos>(off);
    }
    return !docid_to_offset.empty();
}

bool SearchEngine::extractTag(const std::string &xml, const std::string &tag, std::string &out) {
    std::string open = "<" + tag + ">";
    std::string close = "</" + tag + ">";
    auto pos1 = xml.find(open);
    if (pos1 == std::string::npos) return false;
    pos1 += open.size();
    auto pos2 = xml.find(close, pos1);
    if (pos2 == std::string::npos) return false;
    out = xml.substr(pos1, pos2 - pos1);
    return true;
}

bool SearchEngine::readPageByOffset(std::streampos offset, RawPage &out) {
    std::ifstream fin(pages_path);
    if (!fin) return false;
    fin.seekg(offset);
    std::string block;
    block.reserve(2048);
    std::string line;
    while (std::getline(fin, line)) {
        block += line;
        block.push_back('\n');
        if (line == "</doc>") break;
    }
    if (block.empty()) return false;
    extractTag(block, "title", out.title);
    extractTag(block, "link", out.link);
    extractTag(block, "description", out.description);
    return true;
}

bool SearchEngine::readPageByDocId(int docid, RawPage &out) {
    auto it = docid_to_offset.find(docid);
    if (it == docid_to_offset.end()) return false;
    return readPageByOffset(it->second, out);
}

std::string SearchEngine::makeSummary(const std::string &text, const std::vector<std::string> &terms, size_t window) {
    if (text.empty()) return "";
    std::string lower = text;
    for (char &c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    size_t pos = std::string::npos;
    for (const auto &t : terms) {
        std::string tl = t; for (char &c : tl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        size_t p = lower.find(tl);
        if (p != std::string::npos) { if (pos == std::string::npos || p < pos) pos = p; }
    }
    if (pos == std::string::npos) {
        if (text.size() <= window) return text;
        return text.substr(0, window) + "...";
    }
    size_t start = (pos > window/2) ? pos - window/2 : 0;
    size_t end = std::min(text.size(), start + window);
    return (start ? "..." : "") + text.substr(start, end - start) + (end < text.size() ? "..." : "");
}

std::string SearchEngine::escapeJson(const std::string &s) {
    std::string out; out.reserve(s.size());
    for (char ch : s) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

std::vector<SearchResult> SearchEngine::queryRanked(const std::vector<std::string> &terms, size_t top_k) {
    std::vector<SearchResult> results;
    auto ranked = index.searchANDCosineRanked(terms);
    if (ranked.empty()) return results;
    if (top_k && ranked.size() > top_k) ranked.resize(top_k);

    for (const auto &pr : ranked) {
        RawPage pg;
        if (!readPageByDocId(pr.first, pg)) continue;
        SearchResult r;
        r.docid = pr.first;
        r.title = pg.title;
        r.link = pg.link;
        r.summary = makeSummary(pg.description, terms);
        r.score = pr.second;
        results.emplace_back(std::move(r));
    }
    return results;
}


