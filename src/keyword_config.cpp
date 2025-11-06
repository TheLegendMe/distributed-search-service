#include "keyword_config.h"
#include <fstream>
#include <sstream>
#include <algorithm>

static inline void trim(std::string &s) {
    auto notspace = [](int ch){ return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
}

bool loadKeywordConfig(const std::string &path, KeywordConfig &cfg) {
    std::ifstream fin(path);
    if (!fin) return false;
    std::string line;
    while (std::getline(fin, line)) {
        trim(line);
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        trim(key); trim(val);
        if (key == "CANDIDATES_FILE") cfg.candidates_file = val;
        else if (key == "OUTPUT_DIR") cfg.output_dir = val;
    }
    return !cfg.candidates_file.empty() && !cfg.output_dir.empty();
}


