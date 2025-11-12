#include <iostream>
#include <csignal>
#include <wfrest/HttpServer.h>
#include <wfrest/json.hpp>
#include <wfrest/CodeUtil.h>
#include "app_config.h"
#include "keyword_recommender.h"
#include "keyword_dict.h"
#include <filesystem>

using namespace wfrest;
using json = nlohmann::json;

// 全局变量
static HttpServer *g_server = nullptr;
static std::vector<std::string> keyword_words;
static std::vector<uint32_t> keyword_freqs;
static bool dict_loaded = false;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down recommend service...\n";
    if (g_server) {
        g_server->stop();
    }
}

int main(int argc, char **argv) {
    // 默认配置文件路径
    std::string config_path = "./conf/app.conf";
    
    if (argc >= 2) {
        config_path = argv[1];
    }
    
    // 加载配置
    AppConfig config;
    if (!loadAppConfig(config_path, config)) {
        std::cerr << "Warning: Could not load config from " << config_path 
                  << ", using defaults\n";
    }
    
    std::cout << "========================================\n";
    std::cout << "  💡 Recommend Microservice\n";
    std::cout << "========================================\n";
    std::cout << "Config file:   " << config_path << "\n";
    std::cout << "Listen on:     127.0.0.1:8082\n";
    std::cout << "Keyword dir:   " << config.keyword_dict_dir << "\n";
    std::cout << "========================================\n\n";
    
    // 加载关键词字典
    namespace fs = std::filesystem;
    std::error_code ec;
    std::string dict_path = config.keyword_dict_dir;
    if (fs::is_directory(dict_path, ec)) {
        dict_path = (fs::path(dict_path) / "keyword_dict.txt").string();
    }
    
    if (loadKeywordDictFile(dict_path, keyword_words, keyword_freqs)) {
        dict_loaded = true;
        std::cout << "✓ Keyword dictionary loaded: " << keyword_words.size() << " words\n\n";
    } else {
        std::cerr << "✗ Error: Keyword dictionary not found\n";
        return 1;
    }
    
    // 创建 HTTP 服务器
    HttpServer server;
    g_server = &server;
    
    // 注册信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 健康检查端点
    server.GET("/health", [](const HttpReq *req, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json";
        resp->String("{\"status\":\"ok\",\"service\":\"recommend\"}");
    });
    
    // 推荐端点
    server.GET("/recommend", [](const HttpReq *req, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json; charset=utf-8";
        resp->headers["Access-Control-Allow-Origin"] = "*";
        
        json response;
        
        if (!dict_loaded) {
            response["error"] = "Keyword dictionary not loaded";
            response["suggestions"] = json::array();
            resp->String(response.dump());
            return;
        }
        
        std::string query = CodeUtil::url_decode(req->query("q"));
        int topK = 5;
        try {
            std::string topk_str = req->query("topk");
            if (!topk_str.empty()) {
                topK = std::stoi(topk_str);
                if (topK <= 0) topK = 5;
                if (topK > 20) topK = 20;
            }
        } catch (...) {}
        
        if (query.empty()) {
            response["query"] = "";
            response["suggestions"] = json::array();
            resp->String(response.dump());
            return;
        }
        
        // 执行推荐
        auto suggestions = recommendKeywords(query, keyword_words, keyword_freqs, static_cast<size_t>(topK));
        
        // 构建 JSON 响应
        response["query"] = query;
        response["suggestions"] = json::array();
        
        for (const auto &sug : suggestions) {
            json item;
            item["word"] = sug.word;
            item["distance"] = sug.distance;
            item["frequency"] = sug.frequency;
            response["suggestions"].push_back(item);
        }
        
        resp->String(response.dump());
    });
    
    // 字典统计端点
    server.GET("/stats", [](const HttpReq *req, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json";
        resp->headers["Access-Control-Allow-Origin"] = "*";
        
        json response;
        response["loaded"] = dict_loaded;
        response["total_words"] = keyword_words.size();
        
        resp->String(response.dump());
    });
    
    // 启动服务器
    std::cout << "🚀 Recommend service starting...\n";
    if (server.start("0.0.0.0", 8082) == 0) {
        std::cout << "✓ Recommend service ready at http://0.0.0.0:8082 (accessible from network)\n";
        server.wait_finish();
    } else {
        std::cerr << "✗ Failed to start recommend service\n";
        return 1;
    }
    
    std::cout << "Recommend service stopped.\n";
    return 0;
}

