#include "app_config.h"
#include <fstream>
#include <algorithm>

static inline void trim(std::string &s) {
    auto notspace = [](int ch){ return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
}

AppConfig::AppConfig()
    : jieba_dict_dir("/home/oym/cppjieba/cppjieba-5.0.3/dict"),
      input_dir("./input"),
      output_dir("./output"),
      simhash_threshold(3),
      candidates_file(""),
      keyword_output_dir("./docs"),
      index_dir("./output"),
      default_topk(20),
      keyword_dict_dir("./docs"),
      recommend_topk(5),
      web_host("0.0.0.0"),
      web_port(8080),
      enable_cache(true),
      redis_host("127.0.0.1"),
      redis_port(6379),
      cache_capacity(1000),
      cache_ttl(3600) {
}

bool loadAppConfig(const std::string &path, AppConfig &cfg) {
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
        
        if (key == "JIEBA_DICT_DIR") cfg.jieba_dict_dir = val;
        else if (key == "INPUT_DIR") cfg.input_dir = val;
        else if (key == "OUTPUT_DIR") cfg.output_dir = val;
        else if (key == "SIMHASH_THRESHOLD") {
            try { cfg.simhash_threshold = std::stoi(val); } catch (...) {}
        }
        else if (key == "CANDIDATES_FILE") cfg.candidates_file = val;
        else if (key == "KEYWORD_OUTPUT_DIR") cfg.keyword_output_dir = val;
        else if (key == "INDEX_DIR") cfg.index_dir = val;
        else if (key == "DEFAULT_TOPK") {
            try { cfg.default_topk = static_cast<size_t>(std::stoi(val)); } catch (...) {}
        }
        else if (key == "KEYWORD_DICT_DIR") cfg.keyword_dict_dir = val;
        else if (key == "RECOMMEND_TOPK") {
            try { cfg.recommend_topk = static_cast<size_t>(std::stoi(val)); } catch (...) {}
        }
        else if (key == "WEB_HOST") cfg.web_host = val;
        else if (key == "WEB_PORT") {
            try { cfg.web_port = std::stoi(val); } catch (...) {}
        }
        else if (key == "ENABLE_CACHE") {
            cfg.enable_cache = (val == "true" || val == "1" || val == "yes");
        }
        else if (key == "REDIS_HOST") cfg.redis_host = val;
        else if (key == "REDIS_PORT") {
            try { cfg.redis_port = std::stoi(val); } catch (...) {}
        }
        else if (key == "CACHE_CAPACITY") {
            try { cfg.cache_capacity = static_cast<size_t>(std::stoi(val)); } catch (...) {}
        }
        else if (key == "CACHE_TTL") {
            try { cfg.cache_ttl = std::stoi(val); } catch (...) {}
        }
    }
    return true;
}

