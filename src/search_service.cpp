#include <iostream>
#include <csignal>
#include <wfrest/HttpServer.h>
#include <wfrest/json.hpp>
#include <wfrest/CodeUtil.h>
#include "app_config.h"
#include "search_engine.h"
#include "weighted_inverted_index.h"
#include "dynamic_index.h"
#include "tokenizer.h"
#include <filesystem>
#include <fstream>

using namespace wfrest;
using json = nlohmann::json;

// 全局变量
static HttpServer *g_server = nullptr;
static SearchEngine *g_engine = nullptr;
static DynamicInvertedIndex *g_dynamic_index = nullptr;

// 清理无效 UTF-8 字符
std::string cleanUtf8(const std::string &str) {
    std::string result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.size(); ) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if (c < 0x80) {
            result.push_back(str[i++]);
        } else if ((c & 0xE0) == 0xC0 && i + 1 < str.size() &&
                   (str[i+1] & 0xC0) == 0x80) {
            result.append(str, i, 2);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < str.size() &&
                   (str[i+1] & 0xC0) == 0x80 && (str[i+2] & 0xC0) == 0x80) {
            result.append(str, i, 3);
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < str.size() &&
                   (str[i+1] & 0xC0) == 0x80 && (str[i+2] & 0xC0) == 0x80 && (str[i+3] & 0xC0) == 0x80) {
            result.append(str, i, 4);
            i += 4;
        } else {
            i++;
        }
    }
    return result;
}

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down search service...\n";
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
    std::cout << "  🔍 Search Microservice\n";
    std::cout << "========================================\n";
    std::cout << "Config file: " << config_path << "\n";
    std::cout << "Listen on:   127.0.0.1:8081\n";
    std::cout << "Index dir:   " << config.index_dir << "\n";
    std::cout << "========================================\n\n";
    
    // 初始化 Jieba 分词器
    if (!config.jieba_dict_dir.empty()) {
        JiebaTokenizer::instance().initialize(config.jieba_dict_dir);
        std::cout << "✓ Jieba tokenizer initialized\n";
    }
    
    // 加载搜索索引
    namespace fs = std::filesystem;
    std::string index_path = (fs::path(config.index_dir) / "index.txt").string();
    std::string pages_path = (fs::path(config.index_dir) / "pages.bin").string();
    std::string offsets_path = (fs::path(config.index_dir) / "offsets.bin").string();
    
    size_t total_docs = 0;
    std::ifstream fin(offsets_path);
    if (fin) {
        int id;
        long long off;
        while (fin >> id >> off) {
            (void)id; (void)off;
            ++total_docs;
        }
    }
    
    WeightedInvertedIndex index;
    if (total_docs > 0) {
        index.loadFromFile(index_path, total_docs);
        g_engine = new SearchEngine(index, pages_path, offsets_path);
        g_engine->loadOffsets();
        
        // 启用缓存
        if (config.enable_cache) {
            g_engine->enableCache(config.redis_host, 
                                 config.redis_port, 
                                 config.cache_capacity, 
                                 config.cache_ttl);
            std::cout << "✓ Cache enabled: Redis=" << config.redis_host 
                      << ":" << config.redis_port << "\n";
        }
        
        std::cout << "✓ Search index loaded: " << total_docs << " documents\n";
        
        // 初始化动态索引（支持实时更新）
        g_dynamic_index = new DynamicInvertedIndex();
        if (g_dynamic_index->loadFromFile(index_path, total_docs)) {
            std::cout << "✓ Dynamic index initialized (supports real-time updates)\n\n";
        } else {
            std::cout << "⚠ Dynamic index initialization failed, updates disabled\n\n";
            delete g_dynamic_index;
            g_dynamic_index = nullptr;
        }
    } else {
        std::cerr << "✗ Error: Search index not found or empty\n";
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
        resp->String("{\"status\":\"ok\",\"service\":\"search\"}");
    });
    
    // 搜索端点（支持动态索引）
    server.GET("/search", [&config](const HttpReq *req, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json; charset=utf-8";
        resp->headers["Access-Control-Allow-Origin"] = "*";
        
        json response;
        
        // 获取查询参数并URL解码
        std::string query = CodeUtil::url_decode(req->query("q"));
        int topK = 20;
        try {
            std::string topk_str = req->query("topk");
            if (!topk_str.empty()) {
                topK = std::stoi(topk_str);
                if (topK <= 0) topK = 20;
                if (topK > 100) topK = 100;
            }
        } catch (...) {}
        
        if (query.empty()) {
            response["error"] = "Query is empty";
            response["results"] = json::array();
            resp->String(response.dump());
            return;
        }
        
        // 分词
        std::vector<std::string> terms;
        JiebaTokenizer::instance().tokenize(query, terms);
        
        if (terms.empty()) {
            response["query"] = query;
            response["results"] = json::array();
            resp->String(response.dump());
            return;
        }
        
        // 执行搜索：合并静态索引和动态索引的结果
        std::vector<SearchResult> all_results;
        
        // 1. 查询静态索引（SearchEngine）
        if (g_engine) {
            auto static_results = g_engine->queryRanked(terms, static_cast<size_t>(topK * 2));
            all_results = static_results;
        }
        
        // 2. 查询动态索引（DynamicInvertedIndex）
        if (g_dynamic_index) {
            auto dynamic_results = g_dynamic_index->searchANDCosineRanked(terms);
            
            // 合并动态索引的结果（转换为SearchResult格式）
            for (const auto &[docid, score] : dynamic_results) {
                SearchResult sr;
                sr.docid = docid;
                sr.score = score;
                
                // 尝试获取文档元数据
                DynamicInvertedIndex::DocumentMeta meta;
                if (g_dynamic_index->getDocumentMeta(docid, meta)) {
                    sr.title = meta.title.empty() ? "[动态索引] Doc " + std::to_string(docid) : meta.title;
                    sr.summary = meta.summary.empty() ? "通过API动态添加的文档" : meta.summary;
                    sr.link = meta.link.empty() ? "#/doc/" + std::to_string(docid) : meta.link;
                } else {
                    sr.title = "[动态索引] Doc " + std::to_string(docid);
                    sr.summary = "通过API动态添加的文档";
                    sr.link = "#/doc/" + std::to_string(docid);
                }
                
                all_results.push_back(sr);
            }
        }
        
        // 3. 按分数排序并取topK
        std::sort(all_results.begin(), all_results.end(),
                  [](const SearchResult &a, const SearchResult &b) {
                      return a.score > b.score;
                  });
        
        if (all_results.size() > static_cast<size_t>(topK)) {
            all_results.resize(topK);
        }
        
        // 构建 JSON 响应
        response["query"] = query;
        response["count"] = all_results.size();
        response["results"] = json::array();
        response["sources"] = json::object();
        response["sources"]["static_index"] = g_engine != nullptr;
        response["sources"]["dynamic_index"] = g_dynamic_index != nullptr;
        
        for (const auto &r : all_results) {
            json item;
            item["docid"] = r.docid;
            item["score"] = r.score;
            item["title"] = cleanUtf8(r.title);
            item["link"] = cleanUtf8(r.link);
            item["summary"] = cleanUtf8(r.summary);
            response["results"].push_back(item);
        }
        
        resp->String(response.dump());
    });
    
    // 缓存统计端点
    server.GET("/cache/stats", [&config](const HttpReq *req, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json";
        resp->headers["Access-Control-Allow-Origin"] = "*";
        
        json response;
        
        if (!g_engine) {
            response["error"] = "Search engine not initialized";
            resp->String(response.dump());
            return;
        }
        
        size_t local_hits, redis_hits, misses, local_size;
        g_engine->getCacheStats(local_hits, redis_hits, misses, local_size);
        
        size_t total = local_hits + redis_hits + misses;
        double hit_rate = total > 0 ? (double)(local_hits + redis_hits) / total * 100.0 : 0.0;
        
        response["enabled"] = config.enable_cache;
        response["local_hits"] = local_hits;
        response["redis_hits"] = redis_hits;
        response["misses"] = misses;
        response["total_requests"] = total;
        response["hit_rate"] = hit_rate;
        response["local_cache_size"] = local_size;
        
        resp->String(response.dump());
    });
    
    // 清空缓存端点
    server.POST("/cache/clear", [](const HttpReq *req, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json";
        resp->headers["Access-Control-Allow-Origin"] = "*";
        
        json response;
        
        if (!g_engine) {
            response["error"] = "Search engine not initialized";
            response["success"] = false;
            resp->String(response.dump());
            return;
        }
        
        g_engine->clearCache();
        response["success"] = true;
        response["message"] = "Cache cleared successfully";
        
        resp->String(response.dump());
    });
    
    // ========== 动态索引管理端点 ==========
    
    // POST /index/add - 添加文档到索引
    server.POST("/index/add", [](const HttpReq *req, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json";
        resp->headers["Access-Control-Allow-Origin"] = "*";
        
        json response;
        
        if (!g_dynamic_index) {
            response["success"] = false;
            response["error"] = "Dynamic index not available";
            resp->String(response.dump());
            return;
        }
        
        try {
            json body = json::parse(req->body());
            
            if (!body.contains("docid") || !body.contains("text")) {
                response["success"] = false;
                response["error"] = "Missing required fields: docid, text";
                resp->String(response.dump());
                return;
            }
            
            int docid = body["docid"];
            
            // 检查是否提供了完整元数据
            if (body.contains("title") || body.contains("link")) {
                DynamicInvertedIndex::DocumentMeta meta;
                meta.title = body.value("title", "");
                meta.link = body.value("link", "");
                meta.summary = body.value("summary", "");
                meta.text = body["text"];
                
                g_dynamic_index->addDocument(docid, meta);
            } else {
                // 只有文本，使用简单接口
                std::string text = body["text"];
                g_dynamic_index->addDocument(docid, text);
            }
            
            response["success"] = true;
            response["message"] = "Document added to index";
            response["docid"] = docid;
            
        } catch (const std::exception &e) {
            response["success"] = false;
            response["error"] = std::string("Exception: ") + e.what();
        }
        
        resp->String(response.dump());
    });
    
    // DELETE /index/{docid} - 从索引删除文档
    server.DELETE("/index/:docid", [](const HttpReq *req, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json";
        resp->headers["Access-Control-Allow-Origin"] = "*";
        
        json response;
        
        if (!g_dynamic_index) {
            response["success"] = false;
            response["error"] = "Dynamic index not available";
            resp->String(response.dump());
            return;
        }
        
        try {
            int docid = std::stoi(req->param("docid"));
            g_dynamic_index->removeDocument(docid);
            
            response["success"] = true;
            response["message"] = "Document removed from index";
            response["docid"] = docid;
            
        } catch (const std::exception &e) {
            response["success"] = false;
            response["error"] = std::string("Exception: ") + e.what();
        }
        
        resp->String(response.dump());
    });
    
    // PUT /index/{docid} - 更新索引中的文档
    server.PUT("/index/:docid", [](const HttpReq *req, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json";
        resp->headers["Access-Control-Allow-Origin"] = "*";
        
        json response;
        
        if (!g_dynamic_index) {
            response["success"] = false;
            response["error"] = "Dynamic index not available";
            resp->String(response.dump());
            return;
        }
        
        try {
            int docid = std::stoi(req->param("docid"));
            json body = json::parse(req->body());
            
            if (!body.contains("text")) {
                response["success"] = false;
                response["error"] = "Missing required field: text";
                resp->String(response.dump());
                return;
            }
            
            std::string text = body["text"];
            g_dynamic_index->updateDocument(docid, text);
            
            response["success"] = true;
            response["message"] = "Document updated in index";
            response["docid"] = docid;
            
        } catch (const std::exception &e) {
            response["success"] = false;
            response["error"] = std::string("Exception: ") + e.what();
        }
        
        resp->String(response.dump());
    });
    
    // POST /index/batch/add - 批量添加文档（支持元数据）
    server.POST("/index/batch/add", [](const HttpReq *req, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json";
        resp->headers["Access-Control-Allow-Origin"] = "*";
        
        json response;
        
        if (!g_dynamic_index) {
            response["success"] = false;
            response["error"] = "Dynamic index not available";
            resp->String(response.dump());
            return;
        }
        
        try {
            json body = json::parse(req->body());
            
            if (!body.contains("documents") || !body["documents"].is_array()) {
                response["success"] = false;
                response["error"] = "Missing or invalid field: documents (should be array)";
                resp->String(response.dump());
                return;
            }
            
            int added_count = 0;
            for (const auto &doc : body["documents"]) {
                if (!doc.contains("docid") || !doc.contains("text")) {
                    continue;
                }
                
                int docid = doc["docid"];
                
                // 检查是否有完整元数据
                if (doc.contains("title") || doc.contains("link")) {
                    DynamicInvertedIndex::DocumentMeta meta;
                    meta.title = doc.value("title", "");
                    meta.link = doc.value("link", "");
                    meta.summary = doc.value("summary", "");
                    meta.text = doc["text"];
                    
                    g_dynamic_index->addDocument(docid, meta);
                } else {
                    // 只有文本，使用简单接口
                    g_dynamic_index->addDocument(docid, doc["text"].get<std::string>());
                }
                added_count++;
            }
            
            // 不再调用addDocuments，因为它不支持元数据
            // g_dynamic_index->addDocuments(documents);
            
            response["success"] = true;
            response["message"] = "Documents added to index";
            response["count"] = added_count;
            
        } catch (const std::exception &e) {
            response["success"] = false;
            response["error"] = std::string("Exception: ") + e.what();
        }
        
        resp->String(response.dump());
    });
    
    // GET /index/stats - 索引统计信息
    server.GET("/index/stats", [](const HttpReq *req, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json";
        resp->headers["Access-Control-Allow-Origin"] = "*";
        
        json response;
        
        if (!g_dynamic_index) {
            response["available"] = false;
            response["error"] = "Dynamic index not available";
            resp->String(response.dump());
            return;
        }
        
        try {
            auto stats = g_dynamic_index->getStats();
            
            response["available"] = true;
            response["total_docs"] = stats.total_docs;
            response["active_docs"] = stats.active_docs;
            response["deleted_docs"] = stats.deleted_docs;
            response["total_terms"] = stats.total_terms;
            response["needs_compaction"] = g_dynamic_index->needsCompaction();
            
        } catch (const std::exception &e) {
            response["error"] = std::string("Exception: ") + e.what();
        }
        
        resp->String(response.dump());
    });
    
    // POST /index/compact - 压缩索引（清理已删除文档）
    server.POST("/index/compact", [](const HttpReq *req, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json";
        resp->headers["Access-Control-Allow-Origin"] = "*";
        
        json response;
        
        if (!g_dynamic_index) {
            response["success"] = false;
            response["error"] = "Dynamic index not available";
            resp->String(response.dump());
            return;
        }
        
        try {
            auto stats_before = g_dynamic_index->getStats();
            g_dynamic_index->compact();
            auto stats_after = g_dynamic_index->getStats();
            
            response["success"] = true;
            response["message"] = "Index compacted successfully";
            response["docs_removed"] = stats_before.deleted_docs;
            response["active_docs"] = stats_after.active_docs;
            
        } catch (const std::exception &e) {
            response["success"] = false;
            response["error"] = std::string("Exception: ") + e.what();
        }
        
        resp->String(response.dump());
    });
    
    // POST /index/save - 持久化索引到文件
    server.POST("/index/save", [&config](const HttpReq *req, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json";
        resp->headers["Access-Control-Allow-Origin"] = "*";
        
        json response;
        
        if (!g_dynamic_index) {
            response["success"] = false;
            response["error"] = "Dynamic index not available";
            resp->String(response.dump());
            return;
        }
        
        try {
            namespace fs = std::filesystem;
            std::string save_path = (fs::path(config.index_dir) / "index_updated.txt").string();
            
            if (g_dynamic_index->saveToFile(save_path)) {
                response["success"] = true;
                response["message"] = "Index saved successfully";
                response["path"] = save_path;
            } else {
                response["success"] = false;
                response["error"] = "Failed to save index";
            }
            
        } catch (const std::exception &e) {
            response["success"] = false;
            response["error"] = std::string("Exception: ") + e.what();
        }
        
        resp->String(response.dump());
    });
    
    // 启动服务器
    std::cout << "🚀 Search service starting...\n";
    if (server.start("0.0.0.0", 8081) == 0) {
        std::cout << "✓ Search service ready at http://0.0.0.0:8081 (accessible from network)\n";
        server.wait_finish();
    } else {
        std::cerr << "✗ Failed to start search service\n";
        return 1;
    }
    
    std::cout << "Search service stopped.\n";
    delete g_engine;
    delete g_dynamic_index;
    return 0;
}

